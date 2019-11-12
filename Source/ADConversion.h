// ADConversion.h

#ifndef _ADCONVERSION_h
#define _ADCONVERSION_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

class ADConversion
{
 protected:
	 bool started = false;
	 //when true measuring temperature
	 bool temperature = false;
	 //set ADMUX for measurement of voltage with external reference
	 void setADMUXforVoltage();
 public:
	 
	void init();
	//start AD conversion
	void start(bool readTemp = false);
	// return true while ADC is still running and not finished conversion
	bool isConverting();
	// return measured value 0 .. 1023 when conversion is finihed, othervise -1
	// when measurement is done disable AD converter and clear flag started
	int getMeasuredValue();
	// return true when temperature measurement is processing
	bool isTemperatureMeasurement();
};

//extern ADConversionClass ADConversion;

#endif

