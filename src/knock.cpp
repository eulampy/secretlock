#include "Knock.h"

Knock::Knock(byte _led_pin, byte _sensor_pin):
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
byte Knock::WriteSequence(){
	//byte start_flag = 0;
	unsigned long last_knock = millis();

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
	byte wait_release = 1;	// флаг ожидания отпускния кнопки

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

		for(byte i = 0; i < delays_count; i++){
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
				Serial.println("0 OK: first press");
			}
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
				Serial.print(ch_delay_index); Serial.print(" STOP: "); Serial.print(ch_delay);Serial.print(" > ");Serial.println(delays[ch_delay_index] + DELAY_ERROR);
				reset_check_sequence();
				return FAIL;
			}else{
				if(is_knock()){	// снова стук
					if((DELAY_ERROR > delays[ch_delay_index]) || (ch_delay >= delays[ch_delay_index] - DELAY_ERROR)){ // и из условия выше !(ch_delay > delays[ch_delay_index] + DELAY_ERROR)
						if(ch_delay_index < delays_count - 1){
							Serial.print(ch_delay_index); Serial.print(" OK: "); Serial.print(delays[ch_delay_index] - DELAY_ERROR); Serial.print(" < "); Serial.print(ch_delay);Serial.print(" < "); Serial.println(delays[ch_delay_index] + DELAY_ERROR);
							ch_delay_index++;
						}else{	// достигли конца последовательности
							reset_check_sequence();
							Serial.println("TADAAAAAM");
							return SUCCESS;
						}
						ch_last_knock = millis();
						check_sequence = 1;
					}else{	// слишком быстро
						Serial.print(ch_delay_index); Serial.print(" STOP: "); Serial.print(ch_delay);Serial.print(" < ");Serial.println(delays[ch_delay_index] - DELAY_ERROR);
						reset_check_sequence();
					}
				}
			}
			break;
	} // end of switch

	return CHECKING;
}

