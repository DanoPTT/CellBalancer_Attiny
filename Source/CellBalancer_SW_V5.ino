// LifePo4 Cell balancer 
// external reference on pin 5 Aref (2.048V)
// measured Ubat on pin 3 throught divider (Ubat/2)
// sends respone (data) over serial port Pin 6 (TX)
// receives commands and requests over serial receive port Pin 2 (RX)
// read Ubat and temperature and send it to serial (Attiny85 Pin 6 (TX))
// Serial uses inverted logic levels !!! 0 - low, 1 - High
// Because SoftwareSerial implementation can either receive or send
// there can not be used command cumulating serial data.
// commands for given Cell - conatins aleasy address (a), command is etither forwarded (when addess did not match)
// or processed and is send response. Master unit should wait for response or timeout (it cannot send next commnad unitl not response or timeout).
// a (target address 1..G are cells, ( - data for balancer manager
// command:
//		aQUA get actual Ubat in mV and balance state (0/1), response <(aUA9999 B><crc><cr> 
//		aQTA get actual temperature in kelvin, response <(aTA999><crc><cr> - temp is measured only on request, while voltage each second
//		aQBU get starting balance voltage in mV, response <(aBS9999><crc><cr>
//		aQRC get reference correction in mV (+- 9mV), response for positive value <(aRC 9><crc><cr>, for negative <(aRC-9><crc><cr>
//		aQTC get temperature correction in (+- 9), response for positive value <(aTC 9><crc><cr>, for negative <(aTC-9><crc><cr>
//		aQOC get OSCCAL correction in (+- 9), response for positive value <(aOC 9><crc><cr>, for negative <(aOC-9><crc><cr>
//    aSDB - start dynamic balancing, response (aSDBOK<crc><cr>, it switch off automatically after 60 seconds
//		aEDB - end dynamic balancing, response (aEDBOK<crc><cr>
//		aBSV9999 - set start balance voltage, response (aBSVOK<crc><cr>
//		aSRC - set voltage reference corection (-9..9), response (aRCOK<crc><cr>
// 	aSTC - set temperature corection (-99..99), response (aRTOK<crc><cr>
//    aSOC - set OSCCAL correction (-9..9), response (aOCOK<crc><cr>

//		
// debug - when active it send measured voltage each 3 seconds, it sets to off after 1 minute
//		DBG
//
// brodcasted (re-routed) commands as address is used 'Z':
//    SMA0<crc><cr> set all module addres automaticaly modul increments received address(start is 0) set it and then forwards modified SMAa<crc><cr>
//		QMA get module address,it first sends address of module "(MAa<crc><cr>" followed by ZQMA<crc><cr>
//    
//    command but with adress of module. so it would look like ZSMA0 -> ZSMA1 -> .... last 16th module sends ZSMAG 
//   
//    a- is address of module
//    
// all commands and responses must end with correct <CRC><cr>
//uknow commnad is discarded, correct commands and reponses are forwarder from RX to TX without modification
//
// in sleep mode it takes about 135 uA, while 125 uA takes balancer module (reference REF3020 40uA, divider 20+20kOhm 85uA)
// version 4 from 30.3.2019 - oprava vypoctu zapnutia balanceru
// Ihave found that some chips needs to calibrate oscilator - see setup method
// changes in V5 (28.4.2019): 
// 1. balancer state is append to QUA response to save communication
// 2. <cr> - is used only \n instead of \r\n (arduino default writeln) - this also solves problem with eating last char from QUA response (crc char was equal to \0)
// 3. lowered histeresis to 6mv (origanl 8mv)

#include "Helpers.h"
#include "pip_crc.h"
#include "ADConversion.h"
#include "Buffer.h"
#include <avr\sleep.h>
#include <ATTinyCore.h>
#include <SoftwareSerial.h>
#include <Arduino.h>
#include <EEPROM.h>


#define DEBUG


//inverted logic levels, RX pin 3, TX pin 6
// low 0V, high Vcc
SoftwareSerial swserial(3, 1, true); 

//balancer ovladany pinom 7  
#define BALANACER_OUTPUT PB2
//max balancer_voltage mV 3500mV
const uint16_t MAX_BALANCE_VOLTAGE = (uint16_t)3500;
// balancer hysterezis mV 8mV
const uint16_t  BALANCE_HYSTEREZIS =  (uint16_t)6;

// address in EEPROM for module address uses 3 bytes - 3 x duplicated value
// it is recommended not use first byte (0)
// storing in 3 bytes taking two equall
const int MODULE_ADDRESS_EEPROM = 1;
// starting address where is stored reference correction 3 bytes 3 x duplicated value
const int REFERENCE_CORRECTION_EEPROM = 4;
// starting addres of balance voltage in EEPROM, value is stored in two bytes
// 3 times, so together 6 bytes is used
const int BALANCE_VOLTAGE_EEPROM = 7;
// starting address where is stored temp correction 3 bytes 3 x duplicated value
const int TEMPERATURE_CORRECTION_EEPROM = 13;
// starting address where is stored oscilator correction 3 bytes 3 x duplicated value
const int OSCCAL_CORRECTION_EEPROM = 16;

//reference voltage correction, max range +-9
int8_t ref_correction = 0;
//temperature correction max range +-99
int8_t temperature_correction = 0;

//when > 0, send every 2 seconds actual voltage value
int8_t debug_counter = 0;

//when > 0, balancer is on (dynamic balancer)
int8_t balancer_counter = 0;

//oscilator correction OSCCAL
int8_t osccal_corr = 0;

BufferClass rx_data;
// true when we need to schedule temperature measurement
bool schedule_temperature_measurement = false;
// true when we need to schedule voltage measurement
bool schedule_voltage_measurement = false;
// true ak bola poziadavka na zaslanie udajov
bool sendActualVoltageData = false;
//true when asked to send module address
bool sendModuleAddres = false;
////true when asked to send actual balancing state
//bool sendBalancingState = false;


// vysledok posledneho merania
uint16_t actualVoltage = 0;
//vysledok n-1 merani
uint16_t previousVoltage = 0;
//set balance voltage mV
uint16_t balance_voltage = 3450;
// balancer address range 1 .. G  (16 addresses)
char address = '1';
// message processed by all modules
const char BROADCAST_ADDRESS = 'Z';
// Variables for the Sleep/power down modes:
volatile boolean f_wdt = 1;
// class for ADC
ADConversion adc;

// the setup function runs once when you press reset or power the board
void setup() {
	debug_counter = 0;
	balancer_counter = 0;

  swserial.begin(4800);
  pinMode(BALANACER_OUTPUT, OUTPUT);
  digitalWrite(BALANACER_OUTPUT, LOW);
  //read settings from EEPROM
  //module address
  address = readModuleAddressFromEEPROM();
  /*
  //1 do not need correction
  if (address == '2')
    OSCCAL -= 1; //2 needs -1 to run exactly
  else
  if (address == '3')
    OSCCAL += 2; //3 needs +2 to run exactly
  else
  if (address == '4')
    OSCCAL += 3; //4 needs +3 to run exactly
  else
  //5 do not need correction
  if (address == '6')
    OSCCAL += 3; //6 needs +3 to run exactly
  else
  if (address == '7')
    OSCCAL -= 4; //7 needs -4 to run exactly
  else
  if (address == '8')
    OSCCAL += 1; //8 needs +1 to run exactly
  else
  if (address == '9')
    OSCCAL -= 1; //9 needs -1 to run exactly
  else
  if (address == 'A')
    OSCCAL += 1; //A needs +1 to run exactly
  else
  if (address == 'B')
    OSCCAL -= 1; //B needs -1 to run exactly
  else
  if (address == 'C')
    OSCCAL -= 1; //C needs -1 to run exactly
  else //D no corection required
  if (address == 'E')
    OSCCAL += 3; //E needs +3 to run exactly
  else
  if (address == 'F')
    OSCCAL += 6; //F needs +6 to run exactly
  else
  if (address == 'G')
    OSCCAL -= 1; //G needs -1 to run exactly
  */ 

  ref_correction = readReferenceCorrectionFromEEPROM();
  temperature_correction = readTemperatureCorrectionFromEEPROM();
  balance_voltage = readBalanceVoltageFromEEPROM();
  osccal_corr = readOSCCALCorrectionFromEEPROM();
  //apply OSCCAL correction
  OSCCAL += osccal_corr;

#ifdef DEBUG
  swserial.println(F("Ver. 5 (28.4.2019)"));
  swserial.print(F("Address: "));
  swserial.println(address);
  swserial.print(F("RefCor: "));
  swserial.println(ref_correction,10);
  swserial.print(F("TempCor: "));
  swserial.println(temperature_correction, 10);
  swserial.print(F("Balance voltage: "));
  swserial.println(balance_voltage, 10);
  swserial.print(F("OSCAL: "));
  swserial.println(osccal_corr, 10);
  swserial.println(F("Started."));
#endif // DEBUG

  //set watch dog to 8(4s), 7(2s), 6(1s)
  setup_watchdog(6); 
}

// Watchdog Interrupt Service / is executed when watchdog timed out
ISR(WDT_vect) {
	f_wdt = 1;  // set global flag
}

// the loop function runs over and over again forever
void loop()
{
	if (f_wdt == 1)
	{  // wait for timed out watchdog / flag is set when a watchdog timeout occurs
		f_wdt = 0;       // reset flag, othervise processor will reset

		//start ADC - because there can already runing other measurement I use flags
		schedule_voltage_measurement = true;

		if (debug_counter > 0)
		{ //when debugging is on, send each 3 seconds actual voltage
			debug_counter--;
			sendActualVoltageData = (debug_counter % 2) == 1;
		}
		//decrement "timer" of dynamic balancer till zero
		if (balancer_counter > 0)
			balancer_counter--; //next second
	}
	swserial.listen(); //skusal som listen () v setup a blblo, tak tu
	rx_data.readFromSerial(swserial);

	if (rx_data.hasData)
	{
		//for debug only
		//swserial.println( rx_data.data, );
		//is message for this balancer
		if ((rx_data.len > 0) && (rx_data.data[0] == address))
		{
			//process data in input buffer
			if (strncmp_P(rx_data.data + 1, PSTR("QUA"), 3) == 0)
			{
				//asked to get measured voltage
				// clear buffer
				rx_data.clear();
				sendActualVoltageData = true;
			}
			else
			if (strncmp_P(rx_data.data + 1, PSTR("QTA"), 3) == 0)
			{
				//asked to measure temperature - temp is measured only on request
				// clear buffer
				rx_data.clear();
				schedule_temperature_measurement = true;
			}
			else
			if (strncmp_P(rx_data.data + 1, PSTR("QBU"), 3) == 0)
			{
				//asked to get start balancing voltage
				// clear buffer
				rx_data.clear();
				//pripare data - //send data in format <(aBS9999><crc><cr>
				char buff[11] = { '(',' ','B', 'S' };
				buff[1] = address;
				uint16To4Chars(buff + 4, balance_voltage);
				//add crc and \n to data
				appendCRCAndCR(buff, 8);
				//send data - it is buffer not null terminated string 
				//swserial.println(buff);
				swserial.write((const uint8_t *)buff, 11);
			}
			else
			if (strncmp_P(rx_data.data + 1, PSTR("QRC"), 3) == 0)
			{
				//asked to get reference correction voltage
				// clear buffer
				rx_data.clear();
				//pripare data - //send data in format <(aRC 9><crc><cr>
				char buff[9] = { '(',' ','R','C' };
				buff[1] = address;
				int8To2Chars(buff + 4, ref_correction);
				//add crc and \n to data
				appendCRCAndCR(buff, 6);
				//send data
				//swserial.println(buff);
				swserial.write((const uint8_t *)buff, 9);
			}
			else
			if (strncmp_P(rx_data.data + 1, PSTR("QTC"), 3) == 0)
			{
				//asked to get temperature correction
				// clear buffer
				rx_data.clear();
				//pripare data - //send data in format <(aTC 99><crc><cr>
				char buff[10] = { '(',' ','T','C' };
				buff[1] = address;
				int8To3Chars(buff + 4, temperature_correction);
				//add crc and /n to data
				appendCRCAndCR(buff, 7);
				//send data
				//swserial.println(buff);
				swserial.write((const uint8_t *)buff, 10);
			}
			else
			if (strncmp_P(rx_data.data + 1, PSTR("QOC"), 3) == 0)
			{
				//asked to get OSCCAL correction
				// clear buffer
				rx_data.clear();
				char buff[9] = { '(',' ','O','C' };
				buff[1] = address;
				int8To2Chars(buff + 4, osccal_corr);
				//add crc and \n to data
				appendCRCAndCR(buff, 6);
				//send data
				//swserial.println(buff);
				swserial.write((const uint8_t *)buff, 9);
			}
			else
			if (strncmp_P(rx_data.data + 1, PSTR("SRC"), 3) == 0)
			{
				//set reference correction max +- 9  expected format ASRC 9 or for negative ASRC-9
				if ((rx_data.data[5] >= '0') && (rx_data.data[5] <= '9'))
				{
					char corr = rx_data.data[5] - '0';
					//check sign
					if (rx_data.data[4] == '-')
						corr = corr *-1;

					if ((corr > 9) || (corr < -9))
						corr = 0;
					//store reference voltage correction
					ref_correction = corr;
					//store it in EEPROM
					updateReferenceCorrectionInEEPROM(ref_correction);
					//send response OK
					//pripare data - //send data in format <(ARCOK><crc><cr>
					char buff[9] = { '(',' ','R','C','O','K'};
					buff[1] = address;
					//add crc and \n to data
					appendCRCAndCR(buff, 6);
					//send data
					//swserial.println(buff);
					swserial.write((const uint8_t *)buff, 9);
				}
				// clear RX buffer
				rx_data.clear();
			}
			else //set OSCCAL correction
			if (strncmp_P(rx_data.data + 1, PSTR("SOC"), 3) == 0)
			{
				//set OSCCAL correction max +- 9  expected format ASOC 9 or for negative ASOC-9
				if ((rx_data.data[5] >= '0') && (rx_data.data[5] <= '9'))
				{
					char corr = rx_data.data[5] - '0';
					//check sign
					if (rx_data.data[4] == '-')
						corr = corr *-1;

					if ((corr > 9) || (corr < -9))
						corr = 0;
					//store reference OSCCAL correction
					osccal_corr = corr;
					//store it in EEPROM
					updateOSCCALCorrectionInEEPROM(osccal_corr);
					//prejavi sa az po resete !!!
					//send response OK
					//pripare data - //send data in format <(AOCOK><crc><cr>
					char buff[9] = { '(',' ','O','C','O','K' };
					buff[1] = address;
					//add crc and \n to data
					appendCRCAndCR(buff, 6);
					//send data
					//swserial.println(buff);
					swserial.write((const uint8_t *)buff, 9);
				}
				// clear RX buffer
				rx_data.clear();
			}
			else //set temperature correction - range -99 to 99  aSTC  1<crc>
			if (strncmp_P(rx_data.data + 1, PSTR("STC"), 3) == 0)
			{
				//set reference correction max +- 99  expected format from ASTC 99 to for negative ASRC-99
				if ((rx_data.len >= 7) && (rx_data.len <= 9))
				{
					char len = rx_data.len - 6; //4 chars beginning of commnad + 2 chars crc, max 3 chars
					char buf[4] = { '\0' };
					strncpy(buf, rx_data.data + 4, len);
					buf[len] = '\0';
					int corr = atoi(buf);
					if ((corr > 99) || (corr < -99))
					{
						//send response ER cor
						//pripare data - //send data in format <(aTCER><crc><cr>
						char buff[9] = { '(',' ','T','C','E','R' };
						buff[1] = address;
						//add crc and \n to data
						appendCRCAndCR(buff, 6);
						//send data
						//swserial.println(buff);
						swserial.write((const uint8_t *)buff, 9);
					}
					else
					{
						//store reference voltage correction
						temperature_correction = corr;
						//store it in EEPROM
						updateTemperatureCorrectionInEEPROM(temperature_correction);
						//send response OK
						//pripare data - //send data in format <(aTCOK><crc><cr>
						char buff[9] = { '(',' ','T','C','O','K' };
						buff[1] = address;
						//add crc and \n to data
						appendCRCAndCR(buff, 6);
						//send data
						//swserial.println(buff);
						swserial.write((const uint8_t *)buff, 9);
					}
				}
				else
				{
					//send response ER
					//pripare data - //send data in format <(aTCER><crc><cr>
					char buff[9] = { '(',' ','T','C','E','R' };
					buff[1] = address;
					//add crc and \n to data
					appendCRCAndCR(buff, 6);
					//send data
					//swserial.println(buff);
					swserial.write((const uint8_t *)buff, 9);
				}
				// clear RX buffer
				rx_data.clear();
			}
			else
			if (strncmp_P(rx_data.data + 1, PSTR("SDB"), 3) == 0)
			{
				//start dynamic balancig - stops after one minute automatically
				// clear RX buffer
				rx_data.clear();
				//send response OK
				//pripare data - //send data in format <(ARCOK><crc><cr>
				char buff[10] = { '(',' ','S','D','B','O','K' };
				buff[1] = address;
				//add crc to data
				appendCRCAndCR(buff, 7);
				//send data
				//swserial.println(buff);
				swserial.write((const uint8_t *)buff, 10);
				//when counter > 0 balancer is running
				balancer_counter = 60; //60seconds
			}
			else
			if (strncmp_P(rx_data.data + 1, PSTR("EDB"), 3) == 0)
			{
				//stop dynamic balancig if running
				// clear RX buffer
				rx_data.clear();
				//send response OK
				//pripare data - //send data in format <(ARCOK><crc><cr>
				char buff[10] = { '(',' ','E','D','B','O','K' };
				buff[1] = address;
				//add crc to data
				appendCRCAndCR(buff, 7);
				//send data
				//swserial.println(buff);
				swserial.write((const uint8_t *)buff, 10);
				//when counter > 0 balancer is running
				balancer_counter = 0; //0 to stop it immediatelly
			}
			else
			if (strncmp_P(rx_data.data + 1, PSTR("DBG"), 3) == 0)
			{
				//asked to start/stop debug mode
				// clear buffer
				rx_data.clear();
				//when counter > 0 debug is running
				if (debug_counter == 0)
					debug_counter = 60; //60seconds
				else
					debug_counter = 0; //stopped
			}
			else
			{
				//unknown command addresed to this module
				/*swserial.write('(');
				swserial.write(rx_data.data);
				swserial.println('?');*/
				rx_data.clear();

				char buff[10] = { '(',' ','N','A','K' };
				buff[1] = address;
				//add crc to data
				appendCRCAndCR(buff, 5);
				//send data
				//swserial.println(buff);
				swserial.write((const uint8_t *)buff, 8);
			}
		}
		else
			if ((rx_data.len > 0) && (rx_data.data[0] == BROADCAST_ADDRESS))
			{
				//processlocaly
				if (strncmp_P(rx_data.data + 1, PSTR("QMA"), 3) == 0)
				{
					//asked to get module address
					sendModuleAddres = true;
				}
				//problem with receving and sending at ones ...
				//else //balancer actual voltage request
				//if (strncmp_P(rx_data.data + 1, PSTR("QUA"), 3) == 0)
				//{
				//	//asked to get module actual voltage
				//	sendActualVoltageData = true;
				//}
				else //ZSMA0....ZSMAF - automatic module address settings
					if (strncmp_P(rx_data.data + 1, PSTR("SMA"), 3) == 0)
					{
						//set module address
						if (rx_data.len == 7)
						{
							//there would be address there, increment it by one and then use for module
							char adr = rx_data.data[4];
							if (((adr >= '0') && (adr <= '9')) || ((adr >= 'A') && (adr <= 'F')))
							{
								//increment received address by one
								if (adr == '9')
									adr = 'A';
								else
									adr++;

								//correct address, use it
								address = adr;
								updateModuleAddressInEEPROM(address);
								//clear rx
								rx_data.clear();
								//resend command but with updated address
								char buff[8] = { 'Z','S','M','A', ' ' };
								buff[4] = address;
								//add crc and \n to data
								appendCRCAndCR(buff, 5);
								//send data
								//swserial.println(buff);
								swserial.write((const uint8_t *)buff, 8);
							}
						}
					}

				//resend original command data to next module
				if (rx_data.hasData)
					//swserial.println(rx_data.data);
					swserial.write((const uint8_t *)rx_data.data, rx_data.len);

				rx_data.clear();
			}
			else
			{
				//when crc was OK, but address is different just resend
				swserial.write((const uint8_t *)rx_data.data, rx_data.len);
				swserial.write(CR);
				rx_data.clear();
			}
	}
	else
	{
		//Serial input has no data. it's time to send module data
		if (sendActualVoltageData)
		{
			//send data
			//pripare data - //send data in format <(aUA9999><crc><cr>
			char buff[13] = { '(',' ', 'U','A' };
			buff[1] = address;
			uint16To4Chars(buff + 4, actualVoltage);
			//add balancer state after space
			buff[8] = ' ';
			buff[9] = digitalRead(BALANACER_OUTPUT) == HIGH ? '1' : '0';
			//add crc and \n to data
			appendCRCAndCR(buff, 10);
			//send data
			//swserial.println(buff);
			swserial.write((const uint8_t *)buff, 13);
			sendActualVoltageData = false;
		}
		else
		if (sendModuleAddres)
		{
			//pripare data - //send data in format <(ABS9999><crc><cr>
			char buff[7] = { '(',' ','M','A' };
			buff[1] = address;
			//add crc and \n to data
			appendCRCAndCR(buff, 4);
			//send data
			//swserial.println(buff);
			swserial.write((const uint8_t *)buff, 7);
			sendModuleAddres = false;
		}
		//else
		//if (sendBalancingState)
		//{
		//	//pripare data - //send data in format <(BSaS><crc><cr>
		//	char buff[10] = { '(',' ','B','S',' ','\0' };
		//	buff[1] = address;
		//	buff[4] = digitalRead(BALANACER_OUTPUT) == HIGH ? '1' : '0';
		//	//add crc to data
		//	appendCRCAndCR(buff, 5);
		//	//send data
		//	swserial.println(buff);
		//	sendBalancingState = false;
		//}
	}

	//start voltage or temperature measurement when schduled is active
	//clear schedul flag when started
	if (!adc.isConverting())
	{
		if (schedule_voltage_measurement) 
		{
			adc.start();
			schedule_voltage_measurement = false;
		}
		else
		if (schedule_temperature_measurement)
		{
			adc.start(true);
			schedule_temperature_measurement = false;
		}
	}

	if (!adc.isConverting())
	{
		//adc is finished or stopped
		int adcVal = adc.getMeasuredValue();
		if (adcVal >= 0)
		{
			//finished measurement
			if (adc.isTemperatureMeasurement())
			{
				//temperature result correct and send (in kelvin)
				//send data <(aTA999><crc><cr>
				char buff[14] = { '(',' ', 'T','A', '\0','\0' };
				buff[1] = address;
				uint16To3Chars(buff + 4, (adcVal+temperature_correction));
				//add crc to data
				appendCRCAndCR(buff, 7);
				//send data
				//swserial.println(buff);
				swserial.write((const uint8_t *)buff, 10);
				schedule_temperature_measurement = false;
			}
			else
			{
				//voltage measurement result
				//pouzivam externu refernciu 2048mV, delic 1:1
				//result = adc_val * Uref/1024*2= result * 2048L*2/1024
				uint16_t adcVoltage = adcVal * (2048L + ref_correction) / 512L;
				//robim exponencialny rolling priemer za posledne 3 hodnoty
				//inicializacia ked nie je historia
				if (previousVoltage == 0)
					previousVoltage = adcVoltage;

				actualVoltage = (adcVoltage + previousVoltage * 2) / 3;
				previousVoltage = actualVoltage; //odloz aktualnu hodnotu do historie

				//riadene balanceru
				bool auto_on = ((actualVoltage > MAX_BALANCE_VOLTAGE) ||
					(actualVoltage > balance_voltage) ||
					((digitalRead(BALANACER_OUTPUT) == HIGH) && (actualVoltage + BALANCE_HYSTEREZIS) >= balance_voltage)); //hysterezis

				setBalancer(balancer_counter > 0, auto_on);
			}
		}
		//else
		//{
		//	//adc stopped
		//}
	}

	//when nothing to do go sleep
	if ((!sendActualVoltageData) && (!sendModuleAddres) /*&& (!sendBalancingState)*/ && (!adc.isConverting()) && (!rx_data.hasData))
	{
		//sleep and wait for Watchdog or serial interrupt
		system_sleep();  // Set the unit to sleep
	}
} //end of loop


// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms
// 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec
void setup_watchdog(int ii) {

	byte bb;
	int ww;
	if (ii > 9) ii = 9;
	bb = ii & 7;
	if (ii > 7) bb |= (1 << 5);
	bb |= (1 << WDCE);
	ww = bb;
	//always clear the WDRF flag and the WDE control bit in the initialization routine
	MCUSR &= ~(1 << WDRF);
	// start timed sequence
	WDTCR |= (1 << WDCE) | (1 << WDE);
	// set new watchdog timeout value
	WDTCR = bb;
	WDTCR |= _BV(WDIE);
}

// set system into the sleep state 
// system wakes up when watchdog is timed out
void system_sleep() {
	//set power down mode
//	MCUCR |= (1 << SM1);
//	MCUCR &= ~(1 << SM0); replaced by set_sleep_mode

	//disable ADC
	ADCSRA &= ~(1 << ADEN);
	// set sleep mode
	set_sleep_mode(SLEEP_MODE_PWR_DOWN); 
	//  enable sleep 
	sleep_enable();
	// start sleep
	sleep_mode();                        // System actually sleeps here
	// disable sleep
	sleep_disable();                     // System continues execution here when watchdog timed out 
	//switch on ADC - not required it would be done in adc.Start
	//PRR &= ~(1 << PRADC);
	//switch on USI - not used
}

void setBalancer(bool manual_on, bool auto_on)
{
	if (manual_on || auto_on)
		digitalWrite(BALANACER_OUTPUT, HIGH);
	else
		digitalWrite(BALANACER_OUTPUT, LOW);
}

// appends crc and terminate character '\n' to position i
//do not use println becasue it is not null terminated string !!!
// crc can contain '\0' !!!
void appendCRCAndCR(char* buff, const int i)
{
	unsigned short crc = calculate_crc((byte*)buff, i);
	//set crc bytes
	buff[i] = (char)((crc >> 8) & 0xFF);
	buff[i + 1] = (char)(crc & 0xFF);
	//set '\n'
	buff[i + 2] = CR;
}
// read 3 bytes from EEPROM starting on MODULE_ADDRESS_EEPROM
char readModuleAddressFromEEPROM()
{
	char adrs[] = { '1', '1', '1' };
	adrs[0] = EEPROM.read(MODULE_ADDRESS_EEPROM);
	adrs[1] = EEPROM.read(MODULE_ADDRESS_EEPROM + 1);
	adrs[2] = EEPROM.read(MODULE_ADDRESS_EEPROM + 2);

	char addr = 0;
	//find two identical addresses and use them, must be in range '1'..'G'
	if ((adrs[0] == adrs[1]) &&
		(((adrs[0] >= '1') && (adrs[0] <= '9')) || ((adrs[0] >= 'A') && (adrs[0] <= 'G'))))
	{
		addr = adrs[0];
	}
	else
	if ((adrs[1] == adrs[2]) &&
		(((adrs[1] >= '1') && (adrs[1] <= '9')) || ((adrs[1] >= 'A') && (adrs[1] <= 'G'))))
	{
		addr = adrs[1];
	}
	else
	if ((adrs[0] == adrs[2]) &&
		(((adrs[0] >= '1') && (adrs[0] <= '9')) || ((adrs[0] >= 'A') && (adrs[0] <= 'G'))))
	{
		addr = adrs[1];
	}
	//
	if (((addr >= '1') && (addr <= '9')) || ((addr >= 'A') && (addr <= 'G')))
	{
		//correct address do update in EEPROM - for sure
		EEPROM.update(MODULE_ADDRESS_EEPROM, addr);
		EEPROM.update(MODULE_ADDRESS_EEPROM + 1, addr);
		EEPROM.update(MODULE_ADDRESS_EEPROM + 2, addr);
	}
	else
	{
		//use default 1
		addr = '1';
	}

	return addr;
}

// read reference correction from (3 bytes) EEPROM starting on REFERENCE_CORRECTION_EEPROM
//accepted value is -9 ... 9
//when byte is casted to int8_t 
// 1 is stored as 0000 0001
//-1 is stored as 1111 1111 (default EEPROM value)
int8_t readReferenceCorrectionFromEEPROM()
{
	int8_t corrs[3] = { 0,0,0 };
	int8_t corr = 0;
	corrs[0] = EEPROM.read(REFERENCE_CORRECTION_EEPROM);
	corrs[1] = EEPROM.read(REFERENCE_CORRECTION_EEPROM + 1);
	corrs[2] = EEPROM.read(REFERENCE_CORRECTION_EEPROM + 2);
	bool valid = false;
	//find two identical values and use them, must be in range -9..9
	if ((corrs[0] == corrs[1]) && (corrs[0] >= -9) && (corrs[0] <= 9))
	{
		corr = corrs[0];
		valid = true;
	}
	else
	if ((corrs[1] == corrs[2]) && (corrs[1] >= -9) && (corrs[1] <= 9))
	{
		corr = corrs[1];
		valid = true;
	}
	else
	if ((corrs[0] == corrs[2]) && (corrs[0] >= -9) && (corrs[0] <= 9))
	{
		corr = corrs[1];
		valid = true;
	}
	//for sure update in EEPROM
	if (valid && (corr >= -9) && (corr <= 9))
	{
		EEPROM.update(REFERENCE_CORRECTION_EEPROM, corr);
		EEPROM.update(REFERENCE_CORRECTION_EEPROM + 1, corr);
		EEPROM.update(REFERENCE_CORRECTION_EEPROM + 2, corr);
	}

	return corr;
}

// read temperature correction from (3 bytes) EEPROM starting on TEMPERATURE_CORRECTION_EEPROM
//accepted value is -99 ... 99
int8_t readTemperatureCorrectionFromEEPROM()
{
	int8_t corrs[3] = { 0,0,0 };
	int8_t corr = 0;
	corrs[0] = EEPROM.read(TEMPERATURE_CORRECTION_EEPROM);
	corrs[1] = EEPROM.read(TEMPERATURE_CORRECTION_EEPROM + 1);
	corrs[2] = EEPROM.read(TEMPERATURE_CORRECTION_EEPROM + 2);
	bool valid = false;
	//find two identical values and use them, must be in range -99..99
	if ((corrs[0] == corrs[1]) && (corrs[0] >= -99) && (corrs[0] <= 99))
	{
		corr = corrs[0];
		valid = true;
	}
	else
		if ((corrs[1] == corrs[2]) && (corrs[1] >= -99) && (corrs[1] <= 99))
		{
			corr = corrs[1];
			valid = true;
		}
		else
			if ((corrs[0] == corrs[2]) && (corrs[0] >= -99) && (corrs[0] <= 99))
			{
				corr = corrs[1];
				valid = true;
			}
	//for sure update in EEPROM
	if (valid && (corr >= -99) && (corr <= 99))
	{
		updateTemperatureCorrectionInEEPROM(corr);
	}

	return corr;
}

//read OSCALL correction from (3 bytes) EEPROM starting on OSCCAL_CORRECTION_EEPROM
//accepted range is -9 +9
int8_t readOSCCALCorrectionFromEEPROM()
{
	int8_t corrs[3] = { 0,0,0 };
	int8_t corr = 0;
	corrs[0] = EEPROM.read(OSCCAL_CORRECTION_EEPROM);
	corrs[1] = EEPROM.read(OSCCAL_CORRECTION_EEPROM + 1);
	corrs[2] = EEPROM.read(OSCCAL_CORRECTION_EEPROM + 2);
	bool valid = false;
	//find two identical values and use them, must be in range -9..9
	if ((corrs[0] == corrs[1]) && (corrs[0] >= -9) && (corrs[0] <= 9))
	{
		corr = corrs[0];
		valid = true;
	}
	else
	if ((corrs[1] == corrs[2]) && (corrs[1] >= -9) && (corrs[1] <= 9))
	{
		corr = corrs[1];
		valid = true;
	}
	else
	if ((corrs[0] == corrs[2]) && (corrs[0] >= -9) && (corrs[0] <= 9))
	{
		corr = corrs[1];
		valid = true;
	}
	//for sure update in EEPROM
	if (valid && (corr >= -9) && (corr <= 9))
	{
		updateOSCCALCorrectionInEEPROM(corr);
	}

	return corr;
}

//get balance voltage set in eeprom, must be in range 3400 - 3600 mV
//if out of range is used default 3450mV
// in EEPROM is stored only diference to 3399 mV (1..201)
uint16_t readBalanceVoltageFromEEPROM()
{
	uint16_t volt = 0;

	uint8_t ee_val[3] = {0,0,0};

	for (uint8_t i = 0; i < 3; i++)
	{
		ee_val[i] = EEPROM.read(BALANCE_VOLTAGE_EEPROM + i);
	}

	//find two identical values and use them, must be in range 1..201
	if ((ee_val[0] == ee_val[1]) && (ee_val[0] >= 1) && (ee_val[0] <= 201))
	{
		volt = 3399 + ee_val[0];
	}
	else
	if ((ee_val[1] == ee_val[2]) && (ee_val[1] >= 1) && (ee_val[1] <= 201))
	{
		volt = 3399 + ee_val[1];
	}
	else
	if ((ee_val[0] == ee_val[2]) && (ee_val[0] >= 1) && (ee_val[0] <= 201))
	{
		volt = 3399 + ee_val[0];
	}

	if ((volt >= 3400) && (volt <= 3600))
	{ 
		ee_val[0] = (uint8_t)(volt - 3399);
		//correct vales in EEPROM - for sure
		for (uint8_t i = 0; i < 3; i++)
		{
			EEPROM.update(BALANCE_VOLTAGE_EEPROM + i, ee_val[0]);
		}
	}
	else
	{
		//use default
		volt = 3450;//mV
	}

	return volt;
}

// write 3 bytes to EEPROM starting on MODULE_ADDRESS_EEPROM
bool updateModuleAddressInEEPROM(char address)
{
	if (((address >= '1') && (address <= '9')) || ((address >= 'A') && (address <= 'G')))
	{
		EEPROM.update(MODULE_ADDRESS_EEPROM, address);
		EEPROM.update(MODULE_ADDRESS_EEPROM + 1, address);
		EEPROM.update(MODULE_ADDRESS_EEPROM + 2, address);
		return true;
	}
	
	return false;
}

// write 3 bytes to EEPROM starting on REFERENCE_CORRECTION_EEPROM
//accepted value is -9 ... 9
bool updateReferenceCorrectionInEEPROM(int8_t corr)
{
	if ((corr >= -9) && (corr <= 9))
	{
		EEPROM.update(REFERENCE_CORRECTION_EEPROM, corr);
		EEPROM.update(REFERENCE_CORRECTION_EEPROM + 1, corr);
		EEPROM.update(REFERENCE_CORRECTION_EEPROM + 2, corr);
		return true;
	}

	return false;
}

// write 3 bytes to EEPROM starting on TEMPERATURE_CORRECTION_EEPROM
//accepted value is -99 ... 99
bool updateTemperatureCorrectionInEEPROM(int8_t corr)
{
	if ((corr >= -99) && (corr <= 99))
	{
		EEPROM.update(TEMPERATURE_CORRECTION_EEPROM, corr);
		EEPROM.update(TEMPERATURE_CORRECTION_EEPROM + 1, corr);
		EEPROM.update(TEMPERATURE_CORRECTION_EEPROM + 2, corr);
		return true;
	}

	return false;
}

// write 3 bytes to EEPROM starting on OSCCAL_CORRECTION_EEPROM
//accepted value is -9 ... 9
bool updateOSCCALCorrectionInEEPROM(int8_t corr)
{
	if ((corr >= -9) && (corr <= 9))
	{
		EEPROM.update(OSCCAL_CORRECTION_EEPROM, corr);
		EEPROM.update(OSCCAL_CORRECTION_EEPROM + 1, corr);
		EEPROM.update(OSCCAL_CORRECTION_EEPROM + 2, corr);
		return true;
	}
	return false;
}

bool updateBalanceVoltageInEEPROM(uint16_t balanceVoltage)
{
	if ((balanceVoltage >= 3400) && (balanceVoltage <= 3600))
	{
		uint8_t ee_val = (uint8_t)(balanceVoltage - 3399);
		//correct vales in EEPROM - for sure
		for (uint8_t i = 0; i < 3; i++)
		{
			EEPROM.update(BALANCE_VOLTAGE_EEPROM + i, ee_val);
		}
		return true;
	}
	return false;
}
/* end of file */
