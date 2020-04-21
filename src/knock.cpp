#include "Knock.h"


////////////////////////////////////////////////////////////////////////////////
// Конструктор
Knock::Knock(uint8_t _led_pin, uint8_t _sensor_pin):
	led_pin(_led_pin), sensor_pin(_sensor_pin){
	pinMode(led_pin, OUTPUT);
	digitalWrite(led_pin, LOW);
	pinMode(sensor_pin, INPUT);	// вход без подтяжки
};

//////////////////////////////////////////////////////////////
// Запись последовательности стуков/нажатий
// возвращает "1", если последовательность была записана
// "0" - если запись была прервана
// функция останавливает главный цикл
uint8_t Knock::WriteSequence(){
	//uint8_t start_flag = 0;
	unsigned long last_knock = millis();
	delays_count = 0;
	
	// ждем первый стук
	while(1){
		if(is_knock()){
			last_knock = millis();	// есть первый стук
			led_on();
			break;
		}
		if(millis() - last_knock > KNOCK_TIMEOUT)
		{
			led_off();
			return 0;	
		}
	}

	unsigned long knock_delay;
	uint8_t wait_release = 1;	// флаг ожидания отпускния кнопки

	delay(50);	// 50 мс на дребезг 
	
	while(1){	// теперь записываем последовательность стуков
		knock_delay = millis() - last_knock;	// прошедшее время
		if(wait_release){	// если ждем отпускания кнопки
			if(!is_knock()){	// отпустили
				led_off();
				wait_release = 0;
			}else{
				if(knock_delay > KNOCK_TIMEOUT){	// слишком долго нажата
					triple_blink();
					// что-то еще сделать... (стереть записанную последовательность???)
					return 0;
				}
			}
		}else{	// кнопка отпущена, ждем следующего нажатия
			if(is_knock()){	// снова стук
				led_on();
				delays[delays_count] = knock_delay;	// long -> unsigned_int
				delays_count++;
				if(delays_count > MAX_KNOCKS_COUNT - 1){	// если превышено число стуков, считаем что произошла ошибка и последовательность не записывается
					delays_count = 0;
					delay(1000);
					triple_blink();
					return 0;					
				}
				last_knock = millis();
				wait_release = 1;	// флаг ожидания отпускания кнопки
				delay(50);	// 50 мс на дребезг контактов
			}else{	// все еще дждем следующего стука
				if(knock_delay > KNOCK_TIMEOUT){	// долго не нажимается кнопка
					//triple_blink();
					return delays_count;	// запись закончена
				}
			}
		}
	} // end of while(1)
}

////////////////////////////////////////////////////////////////////////////////////////
// "проморгать" подключенным светодиодом записанную последовательность
void Knock::PlaySequence(){
	if(delays_count > 0){ // если что-то вообще записано
		led_on();
		delay(50);
		led_off();

		for(uint8_t i = 0; i < delays_count; i++){
			delay(delays[i] - 50); // тут может получиться очень большая величина, если записанный интервал будет меньше 50 мс
			led_on();
			delay(50);
			led_off();
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// Чтение последовательности
// возвращает SUCCESS
CheckResult Knock::CheckSequence(){
	if(delays_count == 0) return NO_SEQUENCE;	// последовательность не была записана
	ch_delay = millis() - ch_last_knock;
	switch(check_sequence){
		case 0:
			if(is_knock()){
				ch_last_knock = millis();
				check_sequence = 1;		
				if(DEBUG) {Serial.println("0: first press");}
				// return CHECKING
			}
			else return NO_KNOCKING;
			break;
		case 1:	// ждем отпускания кнопки
			if(ch_delay >= 50 && ch_delay <= KNOCK_TIMEOUT){
				if(!is_knock()){	// кнопка уже не нажата
					check_sequence = 2;
				}
			}else if(ch_delay > KNOCK_TIMEOUT){	// кнопка была нажата слишком долго
				reset_check_sequence();
				return FAIL;
			}
			break;
		case 2:	// ждем нажатия кнопки
			if(ch_delay > delays[ch_delay_index] + DELAY_ERROR){	// слишком долго
				if(DEBUG) {Serial.print(ch_delay_index); Serial.print(" STOP: "); Serial.print(ch_delay);Serial.print(" > ");Serial.println(delays[ch_delay_index] + DELAY_ERROR); }
				reset_check_sequence();
				return FAIL;
			}else{
				if(is_knock()){	// снова стук
					if((DELAY_ERROR > delays[ch_delay_index]) || (ch_delay >= delays[ch_delay_index] - DELAY_ERROR)){ // и из условия выше !(ch_delay > delays[ch_delay_index] + DELAY_ERROR)
						if(ch_delay_index < delays_count - 1){
							if(DEBUG) {Serial.print(ch_delay_index); Serial.print(" OK: "); Serial.print(delays[ch_delay_index] - DELAY_ERROR); Serial.print(" < "); Serial.print(ch_delay);Serial.print(" < "); Serial.println(delays[ch_delay_index] + DELAY_ERROR);}
							ch_delay_index++;
						}else{	// достигли конца последовательности
							reset_check_sequence();
							if(DEBUG) {Serial.println("TADAAAAAM");}
							return SUCCESS;
						}
						ch_last_knock = millis();
						check_sequence = 1;
						// return CHECKING;
					}else{	// слишком быстро
						if(DEBUG) {Serial.print(ch_delay_index); Serial.print(" STOP: "); Serial.print(ch_delay);Serial.print(" < ");Serial.println(delays[ch_delay_index] - DELAY_ERROR); }
						reset_check_sequence();
						return FAIL;
					}
				}
			}
			break;
	} // end of switch

	return CHECKING;
}

void Knock::WriteEEPROMData(){
	if(delays_count > 0) {
		if(DEBUG)Serial.print("Write delays to EEPROM...");
		eeprom_write_byte((uint8_t *) (EEPROM_SETTINGS_START_ADDR + sizeof(uint8_t)), delays_count);
		eeprom_write_block((void *) delays, (void *) (EEPROM_SETTINGS_START_ADDR + 2 * sizeof(uint8_t)), delays_count * sizeof(uint16_t));
		if(DEBUG) {Serial.println("done!"); Serial.print((uint8_t) (delays_count * sizeof(uint16_t))); Serial.println(" bytes written.");}
	}
}


uint8_t Knock::ReadEEPROMData(){
	uint8_t somevalue = eeprom_read_byte((uint8_t *) (EEPROM_SETTINGS_START_ADDR + sizeof(uint8_t)));	// чтение числа задержек между стуками
	if(DEBUG)Serial.println("Read delays count from EEPROM...");
	if(somevalue == 0 || somevalue > MAX_KNOCKS_COUNT){
		if(DEBUG){Serial.print("ERROR! - bad delays count: "); Serial.println(somevalue);}
		return 0;
	}
	if(DEBUG){Serial.print("Delays found: ");Serial.println(somevalue);Serial.println("Read delays...");}
	eeprom_read_block((void *) delays, (void *) (EEPROM_SETTINGS_START_ADDR + 2 * sizeof(uint8_t)), somevalue * sizeof(uint16_t));
	// проверка прочитанного массива
	for(uint8_t i = 0; i < somevalue; i++){
		if(delays[i] > KNOCK_TIMEOUT){
			if(DEBUG){Serial.print("ERROR! - bad delay value: "); Serial.println(delays[i]);}
			return 0;
		}
	}
	delays_count = somevalue;
	if(DEBUG){Serial.print("Done! "); Serial.print(delays_count); Serial.println(" delays read.");} 
	return delays_count;
}

