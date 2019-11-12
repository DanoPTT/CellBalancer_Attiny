// Helpers.h

#ifndef _HELPERS_h
#define _HELPERS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif
// converts signed int8 value (9..-9) to 2 chars in buffer, 
//with leading space for positive values
void int8To2Chars(char* buff, char value);

// converts signed int8 value (99..-99) to 3 chars in buffer, 
//with leading space for positive values
void int8To3Chars(char* buff, char value);

// unsigned int16 to 3 chars, with leading space
// requires buffer of len of 4 chars (null terminated string)
void uint16To3Chars(char* buff, uint16_t value);

// unsigned int16 to 4 chars, with leading space
void uint16To4Chars(char* buff, uint16_t value);

/* get 2 byte data as uint16_t - litle endian */
uint16_t byte2Int16(byte* data);

/* uint16_t to 2 byte array  - litle endian */
void int16to2Byte(uint16_t uint16value, byte* data);
#endif

