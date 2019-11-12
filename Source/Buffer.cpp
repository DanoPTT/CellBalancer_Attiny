// 
// 
// 

#include "Buffer.h"

void BufferClass::init()
{
	data[0] = '\0';
	len = 0;
}

bool BufferClass::isFull()
{
	return (len >= BUFFERCLASS_DATA_MAX_LEN);
}

void BufferClass::clear()
{
	hasData = false;
	len = 0;
	data[0] = '\0';
}

bool BufferClass::addChar(const char chr)
{
	if (!isFull())
	{
		data[len] = chr;
		len++;
		data[len] = '\0';
		return true;
	}
	else
		return false;
}
// end of line is '\n' character
bool BufferClass::readFromSerial(Stream &stream)
{
	while (stream.available())
	{
		if (isFull())
		{
			//start from begin of buffer (clear)
			clear();
		}

		char chr = stream.read();
		// if the incoming character is a CR (0x0d), check crc on received data set a flag so the main loop can
		// do something about it:
		if (chr == CR)
		{
			//debug
			/*stream.write((const uint8_t *)data, len);
			stream.write('\n');*/

			//receved message should be at least 3 characters long data + 2chars CRC
			if (len > 2)
			{
				//check crc - uses CRC from PIP (Axpert) - form is <data><CRC><CR>
				unsigned short recievedCrc = data[len - 2];
				recievedCrc = (unsigned short)recievedCrc << 8;
				recievedCrc |= ((byte)data[len - 1]);
				//calclulate crc on data, excluding receved CRC (last two chars)
				unsigned short calculatedCrc = calculate_crc((byte*)data, len - 2);
				/* debug 
				stream.print("Calc CRC: ");
				stream.print((char)((calculatedCrc >> 0) & 0xFF));
				stream.print(" ");
				stream.print((char)((calculatedCrc >> 8) & 0xFF));
				stream.println();
				*/
				if (recievedCrc == calculatedCrc)
				{
					//do not remove crc - required for resending
					//crc is OK, remove CRC with '\0'
					/*data[len - 2] = '\0';
					data[len - 1] = '\0';
					len = len - 2;*/
					hasData = true;
					return true;
				}
			}
				//crc or length is wrong - reset
			clear();
		}
		else
		{
			addChar(chr);
		}
	}
	//no data ready yeat
	return false;
}

/* end of file*/
