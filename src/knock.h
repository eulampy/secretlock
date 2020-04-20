//#pragma once
#include <Arduino.h>
#include <avr/eeprom.h>

// начальный адрес хранения настроек в EEPROM
#define EEPROM_SETTINGS_START_ADDR	0x0A

#ifndef DEBUG
	#define DEBUG	1         // режим отладки 
#endif

enum CheckResult {NO_KNOCKING = 0, CHECKING, NO_SEQUENCE, FAIL, SUCCESS};

class Knock{
private:
// ****** константы	*******
	static const uint16_t KNOCK_TIMEOUT = 5000;	// пауза после стука, означающая конец последовательности
	static const uint8_t MAX_KNOCKS_COUNT = 20;
	static const uint16_t DELAY_ERROR = 100;	// максмальная ошибка в последовательности
public:
// ****** функции ******
	uint8_t ReadEEPROMData();
	void WriteEEPROMData();
	Knock(uint8_t, uint8_t);
	uint8_t WriteSequence();
	void PlaySequence();
	CheckResult CheckSequence();
	inline uint8_t is_knock()	{return (digitalRead(sensor_pin) == HIGH);}
	inline void led_on()	{digitalWrite(led_pin, HIGH);}
	inline void led_off()	{digitalWrite(led_pin, LOW);}
	inline void triple_blink()	{led_on(); delay(50); led_off(); delay(200); led_on(); delay(50); led_off(); delay(200); led_on(); delay(50); led_off(); delay(200); }
// ****** переменные ******
	uint8_t delays_count = 0;
	uint16_t delays[MAX_KNOCKS_COUNT];
// ****** типы ******
	//enum myenum{ ONE, TWO}myenum;
	//CheckResult foo();
private:
	Knock(){};
	uint8_t led_pin, sensor_pin;
	// переменные с префиксом ch_ используются в функции CheckSequence()
	long ch_last_knock = 0; 
	uint8_t ch_delay_index = 0;
	uint8_t check_sequence = 0;
	long ch_delay = 0;
	inline void reset_check_sequence(){	check_sequence = 0;	ch_delay_index = 0;	ch_last_knock = 0; while(is_knock());}

};
