// 
// 
// 

#include "ADConversion.h"

void ADConversion::init()
{
	started = false;

}
//prepare registry for AD conversion, start AD conversion
void ADConversion::start(bool readTemp = false)
{
	//mark flag conversion was started
	started = true;

	temperature = readTemp;

	if (temperature)
	{
		//temperature measurement with internal reference
		// Ref voltage = 1.1V; 0 1 0
		//select the channel and reference
		ADMUX = ((INTERNAL & (0x03)) << REFS0) | ((ADC_TEMPERATURE & (0x0f)) << MUX0); 
		ADMUX |= (((INTERNAL & 0x04) >> 2) << REFS2);
	}
	else
	{  //voltage measurement with external reference
		//ADMUX = 0;
		////pouzi vstup ADC2 (pin 3)
		//ADMUX |= (1 << MUX1);
		////pouzi ext ref na Aref pine (pin 5)
		//ADMUX |= (1 << REFS0);
		setADMUXforVoltage();
	}
	/*Clock   Available prescaler values
	-------------------------------------- -
	1 MHz   8 (125kHz), 16 (62.5kHz)  - bity 1 0 0*/
	// pouzijem 62.5kHz
	ADCSRA =
		(1 << ADEN) |     // Enable ADC 
		(1 << ADPS2) |     // set prescaler to 16, bit 2 
		(0 << ADPS1) |     // set prescaler to 16, bit 1 
		(0 << ADPS0);      // set prescaler to 16, bit 0  
	//start ADC conversion
	ADCSRA |= (1 << ADSC);
	
}

bool ADConversion::isConverting()
{
	return started && bit_is_set(ADCSRA, ADSC);
}
// temperature measurement - take second measurement value
// so if first temp measurement is finished start second measurement
int ADConversion::getMeasuredValue()
{
	if ((started) && (!isConverting()))
	{
		//read result
		uint8_t low = ADCL; // must read ADCL first - it then locks ADCH  
		uint8_t high = ADCH; // unlocks both
		//if ((temperature) && (!secondTempMeasurement))
		//{
		//	//start second measurement of temperature
		//	secondTempMeasurement = true;
		//	//start second ADC conversion
		//	ADCSRA |= (1 << ADSC);
		//}
		//else
		{
			setADMUXforVoltage();
			int result = (high << 8) | low;
			started = false;
			return result;
		}
		
	}
	else //no measurement data ready
		return -1;
}

bool ADConversion::isTemperatureMeasurement()
{
	return temperature;
}


void ADConversion::setADMUXforVoltage()
{
	//reset back external refernce for later voltage measurement
	//ADMUX = 0;
	//pouzi vstup ADC2 (pin 3) ext ref na Aref pine (pin 5)
	ADMUX = (1 << MUX1) | (1 << REFS0);
	//pouzi ext ref na Aref pine (pin 5)
	//ADMUX |= (1 << REFS0);
}

//ADConversionClass ADConversion;

