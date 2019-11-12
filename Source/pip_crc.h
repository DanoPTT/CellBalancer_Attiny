// pip_crc.h

#ifndef _PIP_CRC_h
#define _PIP_CRC_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

unsigned short calculate_crc(byte* pin, byte len);

#endif

