//#pragma once
#include <Arduino.h>

#define debug 1         // режим отладки - вывод в порт информации о процессе игры

enum CheckResult{CHECKING = 0, NO_SEQUENCE, FAIL, SUCCESS};

class Knock{
private:
// ****** константы	*******
	static const unsigned int KNOCK_TIMEOUT = 5000;	// пауза после стука, означающая конец последовательности
	static const byte MAX_KNOCKS_COUNT = 20;
	static const unsigned int DELAY_ERROR = 100;	// максмальная ошибка в последовательности
public:
// ****** функции ******
	Knock(byte, byte);
	byte WriteSequence();
	void PlaySequence();
	CheckResult CheckSequence();
	inline byte is_knock()	{return (digitalRead(sensor_pin) == HIGH);}
	inline void led_on()	{digitalWrite(led_pin, HIGH);}
	inline void led_off()	{digitalWrite(led_pin, LOW);}
	inline void triple_blink()	{led_on(); delay(50); led_off(); delay(200); led_on(); delay(50); led_off(); delay(200); led_on(); delay(50); led_off(); delay(200); }
// ****** переменные ******
	byte public_val;
	byte delays_count = 0;
	unsigned int delays[MAX_KNOCKS_COUNT];
// ****** типы ******
	//enum myenum{ ONE, TWO}myenum;
	//CheckResult foo();
private:
	Knock(){};
	byte led_pin, sensor_pin;
	// переменные с префиксом ch_ используются в функции CheckSequence()
	long ch_last_knock = 0; 
	byte ch_delay_index = 0;
	byte check_sequence = 0;
	long ch_delay = 0;
	inline void reset_check_sequence(){	check_sequence = 0;	ch_delay_index = 0;	ch_last_knock = 0; while(is_knock());}

	/*
	byte fade_count, knock;
	volatile byte mode;
	boolean cap_flag, write_start;
	volatile boolean debonce_flag, threshold_flag;
	volatile unsigned long debounce_time;
	unsigned long last_fade, last_try, last_knock, knock_time, button_time;

	byte count, try_count;
	int delays[KNOCK_MAX_COUNT], min_wait[max_knock], max_wait[max_knock];*/
	
};
