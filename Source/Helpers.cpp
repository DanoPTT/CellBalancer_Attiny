// 
// 
// 

#include "Helpers.h"

// converts signed int8 value (9..-9) to 2 chars in buffer, 
//with leading space for positive values
void int8To2Chars(char* buff, char value)
{
	//corrections
	if (value > 9)
		value = 9;
	else
		if (value < -9)
			value = -9;

	char n;

	/*if (value >= 10)
	{
		n = value / 10;
		buff[0] = n + '0';

		value = value - n * 10;
	}
	else*/
		if (value < 0)
		{
			buff[0] = '-';
			value = value * -1;
		}
		else
			buff[0] = ' ';

	buff[1] = value + '0';
	buff[2] = '\0';
}
// converts signed int8 value (99..-99) to 3 chars in buffer, 
//with leading space for positive values
void int8To3Chars(char* buff, char value)
{
	//corrections
	if (value > 99)
		value = 99;
	else
		if (value < -99)
			value = -99;

	char n; bool zero = false;
	//sign char
	if (value < 0)
	{
		buff[0] = '-';
		value = value * -1;
	}
	else
		buff[0] = ' ';

	//tents
	if (value >= 10)
	{
		n = value / 10;
		buff[1] = n + '0';

		value = value - n * 10;
		zero = true;
	}
	else
		buff[1] = zero ? '0' : ' ';

	buff[2] = value + '0';
	buff[3] = '\0';
}
// unsigned int16 to 4 chars, with leading space
void uint16To4Chars(char* buff, uint16_t value)
{
	//corrections
	if (value > 9999)
		value = 9999;
	else
		if (value < 0)
			value = 0;

	char n; bool zero = false;

	if (value >= 1000)
	{
		n = value / 1000;
		buff[0] = n + '0';

		value = value - n * 1000;
		zero = true;
	}
	else
		buff[0] = ' ';

	if (value >= 100)
	{
		n = value / 100;
		buff[1] = n + '0';

		value = value - n * 100;
		zero = true;
	}
	else
		buff[1] = zero ? '0' : ' ';

	if (value >= 10)
	{
		n = value / 10;
		buff[2] = n + '0';

		value = value - n * 10;
		zero = true;
	}
	else
		buff[2] = zero ? '0' : ' ';

	buff[3] = value + '0';
	buff[4] = '\0';
}
// unsigned int16 to 3 chars, with leading space
// requires buffer of len of 4 chars (null terminated string)
void uint16To3Chars(char* buff, uint16_t value)
{
	//corrections
	if (value > 999)
		value = 999;
	else
		if (value < 0)
			value = 0;

	char n; bool zero = false;

	if (value >= 100)
	{
		n = value / 100;
		buff[0] = n + '0';

		value = value - n * 100;
		zero = true;
	}
	else
		buff[0] = zero ? '0' : ' ';

	if (value >= 10)
	{
		n = value / 10;
		buff[1] = n + '0';

		value = value - n * 10;
		zero = true;
	}
	else
		buff[1] = zero ? '0' : ' ';

	buff[2] = value + '0';
	buff[3] = '\0';
}
/* get 2 byte data as uint16_t - litle endian */
uint16_t byte2Int16(byte* data)
{
	return (uint16_t)(((byte)data[1] << 8) | (byte)data[0]);
}
/* uint16_t to 2 byte array  - litle endian */
void int16to2Byte(uint16_t uint16value, byte* data)
{
	//litle endian
	data[1] = highByte(uint16value);
	data[0] = lowByte(uint16value);
}

/* end of file */