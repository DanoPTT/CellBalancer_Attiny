// 
// 
// 

#include "pip_crc.h"
unsigned short calculate_crc(byte * pin, byte len)
{
	unsigned short crc;
	byte da;
	byte *ptr;
	byte bCRCHign;
	byte bCRCLow;

	const unsigned short crc_ta[16] =
	{
		0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
		0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
	};

	ptr = pin;
	crc = 0;
	while (len-- != 0)
	{
		da = ((byte)(crc >> 8)) >> 4;
		crc <<= 4;
		crc ^= crc_ta[da ^ (*ptr >> 4)];
		da = ((byte)(crc >> 8)) >> 4;
		crc <<= 4;
		crc ^= crc_ta[da ^ (*ptr & 0x0f)];
		ptr++;
	}
	bCRCLow = crc;
	bCRCHign = (byte)(crc >> 8);
	//same like for PIP crc
	if (bCRCLow == 0x28 || bCRCLow == 0x0d || bCRCLow == 0x0a)
	{
		bCRCLow++;
	}
	if (bCRCHign == 0x28 || bCRCHign == 0x0d || bCRCHign == 0x0a)
	{
		bCRCHign++;
	}
	crc = ((unsigned short)bCRCHign) << 8;
	crc += bCRCLow;
	return(crc);
}
