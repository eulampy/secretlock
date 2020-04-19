#include <Arduino.h>
#include <Servo.h>
#include "Knock.h"

#ifndef DEBUG
	#define DEBUG	1
#endif

#if DEBUG == 1 
	#include <GyverEncoder.h>
	#define CLK	7
	#define DT	8
	Encoder enc1(CLK, DT);
#endif

// кнопки
#define LIMIT_PIN		2	// пин концевика на закрытие
#define LIMIT_HOLD_TIME	500	// время удержания концевика
#define EXT_BUTTON_PIN	A4	// внешняя кнопка

// сервопривод
#define SERVOPIN		9
#define CLOSE_SERVO_VAL	100
#define OPEN_SERVO_VAL	30
#define MOSFETPIN		13	// пин управления питанием сервопривода

#define DEBUG_LED_PIN	4

// глобальные переменные и типы
enum StateEnum {WRITE = 0, OPEN, CLOSE};
uint32_t time;
StateEnum g_state;
volatile uint32_t limit_press_time = 0;	// время нажатия концевика

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
			delay(100);
			s.write(open_val);
			delay(100);
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
Knock knock(DEBUG_LED_PIN, EXT_BUTTON_PIN);

void setup() {
#if DEBUG == 1
	enc1.setType(TYPE1);
#endif

	Serial.begin(9600);
	
	time = millis();
	
	// кнопки
	pinMode(LIMIT_PIN, INPUT_PULLUP);

	uint8_t somevalue = eeprom_read_byte((uint8_t *) 0); // читаем значение g_state из EEPROM
	if(somevalue > CLOSE || somevalue == WRITE){ // прочиталась фигня
		if(DEBUG){Serial.print("Bad g_state value from EEPROM: "); Serial.println(somevalue); Serial.println("g_state = WRITE");}
		g_state = WRITE;
	}else{	// прочиталось правильно
		// пробуем прочитать значения задержек
		if(knock.ReadEEPROMData()){	// если задержки прочитались
			g_state = (StateEnum) somevalue;
			// на всякий случай приведем в соответствие состояние замка в программе и в реальности
			if(g_state == OPEN) safelock.open();
			else safelock.close();

			if(DEBUG){
				if(g_state == OPEN) Serial.println("g_state = OPEN");
				else Serial.println("g_state = CLOSE");
			}
		}else{
			g_state = WRITE;
			if(DEBUG) Serial.println("g_state = WRITE");
		}
	}
}

// сохранить состояние замка в EEPROM
inline void WriteStateToEEPROM(){
	eeprom_write_byte((uint8_t *) 0, (uint8_t) g_state);
}

void loop() {
	static uint8_t led_switch_flag = 1;
	static long led_switch_time = 0;
	static uint8_t write_step = 0;
#if DEBUG == 1
	enc1.tick();	// обязательная функция отработки. Должна постоянно опрашиваться
#endif
	switch(g_state){
		case WRITE:{	// запись последовательности
			if(write_step == 0){	// ждем нажатия кнопки, призывно моргаем светодиодом
				if(knock.is_knock()){	// дождались нажатия
					knock.led_off();
					//while(knock.is_knock());	// ждем отпускания кнопки (TODO: сделать это отдельным шагом g_state, что бы не фризить выполнение главного цикла)
					delay(50);
					write_step = 1;
				}else{
					long some_delay = millis() - led_switch_time;
					if(!led_switch_flag){
						if(some_delay > 50){
							knock.led_off(); //Serial.println("led_off");
							led_switch_time = millis();
							led_switch_flag = 1;
						}
					}else{
						if(some_delay > 450){
							knock.led_on();	//Serial.println("led_on");
							led_switch_time = millis();
							led_switch_flag = 0;
						}
					}
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
					}
<<<<<<< HEAD
					//Serial.println("start PlaySequence()");
					//door.bzzz();
					knock.PlaySequence();
					//Serial.println("end PlaySequence()");
					//door.bzzz();
					Serial.println("g_state = OPEN");
=======
					knock.WriteEEPROMData();
>>>>>>> c77edc35c02ad24c87a0425b8b8fb10f619b5a6d
					g_state = OPEN;
				}else{
					if(DEBUG) Serial.println("no knock sequence");
				}
				write_step = 0; //Serial.println("write_step = 0");
				delay(1000);
			}
		} break;
		case OPEN:{
			if(digitalRead(LIMIT_PIN) == LOW){
				if(limit_press_time == 0){ // отсчет времени нажатия концевика еще не начался
					limit_press_time = millis();
				}else{ // отсчет идет
					if(millis() - limit_press_time > LIMIT_HOLD_TIME){	// концевик зажат достаточно времени
						limit_press_time = 0;
						safelock.close();
						g_state = CLOSE; WriteStateToEEPROM();
						if(DEBUG){
							Serial.println("g_state = CLOSE");
							Serial.println("Door close...");
						}
						delay(1000);
					}
				}
			}else{
				if(limit_press_time > 0) limit_press_time = 0;
			}
		} break;
		case CLOSE:{
			// просто будет вызов тут
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
				// действие в случае успеха
			}else if(check_result == FAIL){
				while(knock.is_knock());	// ждем отпускания кнопки
			}
		} break;
	} // end of switch

#if DEBUG == 1	
	if (enc1.isLeft()) {
		safelock.close();
		g_state = CLOSE; WriteStateToEEPROM();
	}
	if (enc1.isRight()) {
		safelock.open();
		g_state = OPEN; WriteStateToEEPROM();
	}
#endif

/*
	if((millis() - time > 500) && (g_state != WRITE))
	{
		time = millis();
		Serial.print("V = ");
		Serial.println(readVcc());		
		Serial.print("door_state = ");
		Serial.println(safelock.state);		
	}
	*/
}

float my_vcc_const = 1.0867;        // начальное значение константы вольтметра

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
