#include <Arduino.h>
#include <GyverEncoder.h>
#include <Servo.h>
#include "Knock.h"

#define CLK	7
#define DT 	8
Encoder enc1(CLK, DT);

// кнопки
#define LIMIT_PIN		2	// пин концевика на закрытие
#define LIMIT_HOLD_TIME	500	// время удержания концевика
#define EXT_BUTTON_PIN	A4	// внешняя кнопка

// сервопривод
#define SERVOPIN		9
#define CLOSE_SERVO_VAL	100
#define OPEN_SERVO_VAL	30
#define MOSFETPIN		13	// пин питания сервопривода

#define DEBUG_LED_PIN	4

// глобальные переменные
unsigned long time;
volatile enum {WRITE = 0, OPEN, CLOSE} g_state;
volatile unsigned long limit_press_time = 0;	// время нажатия концевика
byte first_run = 1;

//#define work_time 1000

// хочу свой класс с блекджеком и шлюхами
class MyMegaFckngDoorClass{
private:
	Servo s;
	byte servo_pin; 
	byte mosfet_pin;
	byte close_val;
	byte open_val;
	static const unsigned int work_time = 1000; 
	int pos;
public:
	byte state = 1;	// 0 - закрыто, 1 - открыто
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
		if(state){
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
	MyMegaFckngDoorClass(){};
	MyMegaFckngDoorClass(byte _servo_pin, byte _mosfet_pin, byte _close_val, byte _open_val):
		servo_pin(_servo_pin), mosfet_pin(_mosfet_pin), close_val(_close_val), open_val(_open_val){
		pinMode(mosfet_pin, OUTPUT);
		digitalWrite(mosfet_pin, LOW);
	}
};


MyMegaFckngDoorClass door(SERVOPIN, MOSFETPIN, CLOSE_SERVO_VAL, OPEN_SERVO_VAL);
Knock knock(DEBUG_LED_PIN, EXT_BUTTON_PIN);

void setup() {

	//door = MyMegaFckngDoorClass(SERVOPIN, MOSFETPIN, CLOSE_SERVO_VAL, OPEN_SERVO_VAL);
	enc1.setType(TYPE1);

	Serial.begin(9600);
	//pinMode(DEBUG_LED_PIN, OUTPUT);
	//digitalWrite(DEBUG_LED_PIN, LOW);
	
	time = millis();
	
	// кнопки
	pinMode(LIMIT_PIN, INPUT_PULLUP);

	if(first_run){
		g_state = WRITE;
		Serial.println("g_state = WRITE");
		Serial.println("Write sequence...");
	}else{
		g_state = OPEN;
		Serial.println("g_state == OPEN");
	}
	
	// Serial.println("Run program...");
}

void loop() {
	static byte led_switch_flag = 1;
	static long led_switch_time = 0;
	static byte write_step = 0;

	enc1.tick();	// обязательная функция отработки. Должна постоянно опрашиваться
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
					// пробуем вывести все это в консоль
					Serial.print("delays_count: ");	Serial.println(knock.delays_count);
					Serial.println("i\tdelay(ms)\n--------------------------");
					for(byte i = 0; i < knock.delays_count; i++){
						Serial.print(i);	Serial.print("\t");	Serial.println(knock.delays[i]);
					}
					//Serial.println("start PlaySequence()");
					//door.bzzz();
					knock.PlaySequence();
					//Serial.println("end PlaySequence()");
					//door.bzzz();
					Serial.println("g_state = OPEN");
					g_state = OPEN;
				}else{
					Serial.println("no knock sequence");
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
						door.close();
						g_state = CLOSE;
						Serial.println("g_state = CLOSE");
						Serial.println("Door close...");
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
				Serial.println("CheckSequence() returned TRUE");
				Serial.println("g_state = OPEN");
				Serial.println("Open...");
				door.open();
				g_state = OPEN;
				delay(1000);
				// действие в случае успеха
			}else if(check_result == FAIL){
				while(knock.is_knock());	// ждем отпускания кнопки
			}


			/*if(digitalRead(EXT_BUTTON_PIN) == HIGH){
				door.open();
				g_state = OPEN;
			}*/
		} break;
	} // end of switch

	
	
	if (enc1.isLeft()) {
		door.close();
		g_state = CLOSE;
	}
	if (enc1.isRight()) {
		door.open();
		g_state = OPEN;
	}
/*
	if((millis() - time > 500) && (g_state != WRITE))
	{
		time = millis();
		Serial.print("V = ");
		Serial.println(readVcc());		
		Serial.print("door_state = ");
		Serial.println(door.state);		
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
