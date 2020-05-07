#include <Arduino.h>
#include <Servo.h>
#include <LowPower.h>
#include "Knock.h"

// кнопки
#define LIMIT_PIN		3	// пин концевика на закрытие
#define LIMIT_HOLD_TIME	500	// время удержания концевика
#define EXT_BUTTON_PIN	2	// внешняя кнопка (INT0)
#define EXT_INTERRUPT_NUM	0	// номер внешнего прерывания для кнопки
#define EMERGENCY_OPEN_BUTTON_PIN	7 // спрятанная кнопка аварийного открытия

// сервопривод
#define SERVOPIN		9
#define CLOSE_SERVO_VAL	100
#define OPEN_SERVO_VAL	30
#define MOSFETPIN		13	// пин управления питанием сервопривода

#define GREEN_LED_PIN	4
#define RED_LED_PIN		5


// глобальные переменные и типы
enum ButtonStateEnum {RELEASED = 0, PRESSED, WAIT_AFTER_RELEASE};
enum DeviceStateEnum {WRITE = 0, OPEN, CLOSE} g_state;

//uint32_t time; используется для вывода напряжения питания в консоль

volatile uint32_t limit_press_time = 0;	// время нажатия 
uint32_t idle_time = 0;			// время последнего действия с внешней кнопкой
uint8_t red_led_blink = 0;		// флаг включения мигания красного светодиода (обрабатывается в функции  red_led_blinking())

// хочу свой класс с блекджеком и шлюхами
class Lock{
private:
	Servo s;
	uint8_t servo_pin; 
	uint8_t mosfet_pin;
	uint8_t close_val;
	uint8_t open_val;
	static const uint16_t work_time = 1000; 
	int pos;
public:
	uint8_t state = 1;	// 0 - закрыто, 1 - открыто
	void open(){
		if(!state){
			s.attach(servo_pin);
			digitalWrite(mosfet_pin, HIGH);
			s.write(open_val);
			delay(work_time);
			digitalWrite(mosfet_pin, LOW);
			s.detach();
			state = 1;
		}
	}
	void close(){
		if(state){
			s.attach(servo_pin);
			digitalWrite(mosfet_pin, HIGH);
			s.write(close_val);
			delay(work_time);
			digitalWrite(mosfet_pin, LOW);
			s.detach();
			state = 0;
		}
	}
	// делаем "вжик" замком
	void bzzz(){
		if(state){ // "вжикать" можно только в открытом состоянии
			s.attach(servo_pin);
			digitalWrite(mosfet_pin, HIGH);
			s.write(open_val+5);
			delay(50);
			s.write(open_val);
			delay(50);
			digitalWrite(mosfet_pin, LOW);
			s.detach();
		}
	}
	Lock(){};
	Lock(uint8_t _servo_pin, uint8_t _mosfet_pin, uint8_t _close_val, uint8_t _open_val):
		servo_pin(_servo_pin), mosfet_pin(_mosfet_pin), close_val(_close_val), open_val(_open_val){
		pinMode(mosfet_pin, OUTPUT);
		digitalWrite(mosfet_pin, LOW);
	}
};

Lock safelock(SERVOPIN, MOSFETPIN, CLOSE_SERVO_VAL, OPEN_SERVO_VAL);
Knock knock(GREEN_LED_PIN, EXT_BUTTON_PIN);

// сохранить состояние замка в EEPROM
inline void WriteStateToEEPROM(){
	eeprom_write_byte((uint8_t *) EEPROM_SETTINGS_START_ADDR, (uint8_t) g_state);
}

void WakeUpHandler(){}	// обработчик прерывания от внешней кнопки

inline void sleep(){
	if (DEBUG) {Serial.println("SLEEP");delay(50);}
	delay(5);
	//attachInterrupt(EXT_INTERRUPT_NUM, WakeUpHandler, RISING);	// включаем прерывание по положительному фронту на порту кнопки
  	LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);    // спать. mode POWER_OFF, АЦП выкл
}

inline void red_led_on(){digitalWrite(RED_LED_PIN, HIGH);}
inline void red_led_off(){digitalWrite(RED_LED_PIN, LOW);}

/*********************** прототипы функций ************************************/
void red_led_blinking();	// моргание красным светодиодом раз в секунду
void green_led_blinking();	// моргание зеленым светодиодом
long readVcc();				// чтение напряжение питания
void check_vcc();			// проверка напряжения питания
uint8_t is_10secs_hold_down();	// проверка на удерживание кнопки больше 10 секунд
uint8_t emergency_button_check();	// нажатие аварийной кнопки открытия


void setup() {

	if(DEBUG) Serial.begin(9600);
	
	// кнопки
	pinMode(LIMIT_PIN, INPUT_PULLUP);
	pinMode(EMERGENCY_OPEN_BUTTON_PIN, INPUT_PULLUP);
	
	// светодиоды
	pinMode(RED_LED_PIN, OUTPUT);

	// на время инициализации зажжем все светодиоды
	red_led_on();
	knock.led_on();
	
	//pinMode(GREEN_LED_PIN, OUTPUT);

	uint8_t somevalue = eeprom_read_byte((uint8_t *) EEPROM_SETTINGS_START_ADDR); // читаем значение g_state из EEPROM
	if(somevalue > CLOSE || somevalue == WRITE){ // прочиталась фигня
		if(DEBUG){Serial.print("Bad g_state value from EEPROM: "); Serial.println(somevalue); Serial.println("g_state = WRITE");}
		g_state = WRITE;
	}else{	// прочиталось правильно
		// пробуем прочитать значения задержек
		if(knock.ReadEEPROMData()){	// если задержки прочитались
			g_state = (DeviceStateEnum) somevalue;
			// на всякий случай приведем в соответствие состояние замка в программе и в реальности
			if(g_state == OPEN) safelock.open();
			else {safelock.close(); idle_time = millis();}

			if(DEBUG){
				if(g_state == OPEN) Serial.println("g_state = OPEN");
				else Serial.println("g_state = CLOSE");
			}
		}else{
			g_state = WRITE;
			if(DEBUG) Serial.println("g_state = WRITE");
			safelock.open();	// если вдруг замок был закрыт, открываем его
		}
	}
	check_vcc();	// проверка напряжения
	
	attachInterrupt(EXT_INTERRUPT_NUM, WakeUpHandler, RISING);	// включаем прерывание по положительному фронту на порту кнопки

	// конец инициализации
	red_led_off();
	knock.led_off();
}


void loop() {
	static uint8_t write_step = 0;
	
	// ************** проверка нажатия аварийной кнопки открытия **************
	if(emergency_button_check() && g_state != WRITE){
		if(DEBUG)Serial.println("emergency button pressed, g_state = OPEN");
		safelock.open();
		if(g_state != OPEN) {g_state = OPEN; WriteStateToEEPROM();}
		delay(1000);
		check_vcc();	// проверим напряжение батареек
	}


	switch(g_state){
		case WRITE:{	// запись последовательности
			if(write_step == 0){	// ждем нажатия кнопки, призывно моргаем светодиодом
				green_led_blinking();
				
				if(knock.is_knock()){	// дождались нажатия
					knock.led_off();
					delay(50);
					write_step = 1;
				}
			}else{ // write_step == 1 -> запись последовательности
				if(knock.WriteSequence() > 0){ // что-то записалось
					if(DEBUG){
						// пробуем вывести все это в консоль
						Serial.print("delays_count: ");	Serial.println(knock.delays_count);
						Serial.println("i\tdelay(ms)\n--------------------------");
						for(uint8_t i = 0; i < knock.delays_count; i++){
							Serial.print(i);	Serial.print("\t");	Serial.println(knock.delays[i]);
						}
						//Serial.println("start PlaySequence()");
						//safelock.bzzz();
						knock.PlaySequence();
						//Serial.println("end PlaySequence()");
						//safelock.bzzz();
						Serial.println("g_state = OPEN");
					}else{
						knock.led_on();
						safelock.bzzz();	// функция "работает" 100 мс
						knock.led_off();

						for(uint8_t i = 0; i < knock.delays_count; i++){
							delay(knock.delays[i] < 100 ? 100: knock.delays[i] - 100);
							knock.led_on();
							safelock.bzzz();
							knock.led_off();
						}
					}
						
					knock.WriteEEPROMData();
					g_state = OPEN; WriteStateToEEPROM();
				}else{
					if(DEBUG) Serial.println("no knock sequence");
				}
				write_step = 0; //Serial.println("write_step = 0");
				delay(1000);
			}
		} break;
		case OPEN:{
			red_led_blinking();	// если надо, то мигаем красным светодиодом
			if(is_10secs_hold_down()){	// кнопка удерживается больше 10 секунд
				safelock.bzzz();	// вжикнем, что бы отпустили кнопку
				red_led_off();
				if(DEBUG){Serial.println("Button holding for 10 secs"); Serial.println("g_state = WRITE");}
				g_state = WRITE; WriteStateToEEPROM();
				while(knock.is_knock());	// ждем отпускания кнопки
				delay(1000);	// и еще паузу в 1 секунду
			}

			if(digitalRead(LIMIT_PIN) == LOW){
				if(limit_press_time == 0){ // отсчет времени нажатия концевика еще не начался
					limit_press_time = millis();
				}else{ // отсчет идет
					if(millis() - limit_press_time > LIMIT_HOLD_TIME){	// концевик зажат достаточно времени
						limit_press_time = 0;
						safelock.close();
						g_state = CLOSE; WriteStateToEEPROM();
						red_led_off();
						if(DEBUG){
							Serial.println("g_state = CLOSE");
							Serial.println("Door close...");
						}
						idle_time = millis();	// начинаем отсчет таймера сна
						delay(1000);
					}
				}
			}else{
				if(limit_press_time > 0) limit_press_time = 0;
			}
		} break;
		case CLOSE:{

			// ************** считаем время без нажатий **************
			if(knock.is_knock()){ idle_time = millis(); }	// запоминаем время последнего стука
			else if(millis() - idle_time > 10000){
				sleep();
			}

			// ************** ожидание/распознавание последовательности **************
			CheckResult check_result = knock.CheckSequence();
			if(check_result == SUCCESS){
				if(DEBUG){
					Serial.println("CheckSequence() returned TRUE");
					Serial.println("g_state = OPEN");
					Serial.println("Open...");
				}
				safelock.open();
				g_state = OPEN; WriteStateToEEPROM();
				delay(1000);
				
				// проверим напряжение батареек
				check_vcc();

			}else if(check_result == FAIL){
				while(knock.is_knock());	// ждем отпускания кнопки
			}
		} break;
	} // end of switch

#if DEBUG == 1	
	// if((millis() - time > 500) && (g_state != WRITE))
	// {
	// 	time = millis();
	// 	Serial.print("V = ");
	// 	Serial.println(readVcc());		
	// }
#endif

} // end of loop()


////////////////////////////////////////////////////////////////////////////////
// проверка на удерживание кнопки больше 10 секунд
uint8_t is_10secs_hold_down(){
	static uint32_t hold_down_time = 0;
	static ButtonStateEnum func_state = RELEASED;

	switch (func_state) {
		case RELEASED:
			if(knock.is_knock()){
				hold_down_time = millis();
				func_state = PRESSED;
				knock.led_on();
			}
			break;
		case PRESSED:
			if(knock.is_knock()){	// если кнопка все еще нажата
				if(millis() - hold_down_time > 10000){	// и нажата больше 10 секунд
					func_state = RELEASED;	// паузу не делаем
					return 1;	
				}
			}else{ // отпустили раньше
				func_state = WAIT_AFTER_RELEASE;	// сделаем паузу
				hold_down_time = millis();
				knock.led_off();
			}
		break;
		case WAIT_AFTER_RELEASE:
			if(millis() - hold_down_time > 1000) func_state = RELEASED;
	}	// enf of switch

	return 0;
}



////////////////////////////////////////////////////////////////////////////////
// переключение красного светодиода раз в секунду
void red_led_blinking(){
	static unsigned long last_blink_time = 0;
	static uint8_t led_state = 1;
	if(red_led_blink){
		if(millis() - last_blink_time > 1000){
			led_state = !led_state;
			digitalWrite(RED_LED_PIN, led_state);
			last_blink_time = millis();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// переключение зеленого светодиода 
void green_led_blinking(){
	static unsigned long last_blink_time = 0;
	static uint8_t led_state = 0;
	if(!led_state){	// светодиод выключен
		if(millis() - last_blink_time > 450){
			led_state = !led_state;
			knock.led_on();
			last_blink_time = millis();
		}
	}else{	// светодиод включен
		if(millis() - last_blink_time > 50){
			led_state = !led_state;
			knock.led_off();
			last_blink_time = millis();
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
// проверка ужержания аварийной кнопки открытия
uint8_t emergency_button_check(){
	const uint16_t c_holding_time = 3000;	// необходимое время удержания
	static ButtonStateEnum func_state = RELEASED;
	static uint32_t hold_down_time = 0;

	switch (func_state) {
		case RELEASED:
			if(digitalRead(EMERGENCY_OPEN_BUTTON_PIN) == LOW){
				hold_down_time = millis();
				func_state = PRESSED;
			}
			break;
		case PRESSED:
			if(digitalRead(EMERGENCY_OPEN_BUTTON_PIN) == LOW){	// если кнопка все еще нажата
				if(millis() - hold_down_time > c_holding_time){	
					func_state = WAIT_AFTER_RELEASE;	// сделаем паузу
					hold_down_time = millis();
					return 1;
				}
			}else{ // отпустили раньше
				func_state = RELEASED;	
			}
		break;
		case WAIT_AFTER_RELEASE:
			if(millis() - hold_down_time > 1000) func_state = RELEASED;
	}	// enf of switch

	return 0;
}


const float my_vcc_const = 1.095;		// начальное значение константы вольтметра
const long low_battery_vvc = 3500;		// минимальное значение питания

long readVcc() { //функция чтения внутреннего опорного напряжения, универсальная (для всех ардуин)
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
	ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
	ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
	ADMUX = _BV(MUX3) | _BV(MUX2);
#else
	ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif
	delay(2); // Wait for Vref to settle
	ADCSRA |= _BV(ADSC); // Start conversion
	while (bit_is_set(ADCSRA, ADSC)); // measuring
	uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
	uint8_t high = ADCH; // unlocks both
	long result = (high << 8) | low;

	result = my_vcc_const * 1023 * 1000 / result; // расчёт реального VCC
	return result; // возвращает VCC
}

inline void check_vcc(){
	if(readVcc() < low_battery_vvc) red_led_blink = 1;
	else red_led_blink = 0;
	if(DEBUG){Serial.print("Check VCC: red_led_blink == ");Serial.println(red_led_blink);}
}

