// Buffer.h

#ifndef _BUFFER_h
#define _BUFFER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "pip_crc.h"

//max length of buffer
#define BUFFERCLASS_DATA_MAX_LEN 16

//define CR character 0x0d (10)
#define CR 0x0d

class BufferClass
{
 protected:


 public:
	 //buffer for serial RX
	 char data[BUFFERCLASS_DATA_MAX_LEN +1] = {'\0'};
	 byte len = 0;
	 //true when checked data in data are ready for processing 
	 bool hasData = false;
	void init();

	bool isFull();
	void clear();
	bool addChar(const char chr);
	// check wheter there are data in stream (SoftwareSerial for example) 
	//and store them in this buffer
	// checks existence '\r' or '\n' as end of command  
	// return true when data are ready
	bool readFromSerial(Stream &stream);
};

#endif
