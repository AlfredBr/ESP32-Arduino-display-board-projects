#define DEBUG 0
#include "Arduino.h"
#include "SPI.h"
#include "LED.h"
#include "Debug.h"

const int chipSelect = 10;
const int baud = 9600;
int count = 0;
SPISettings spiSettings(baud, MSBFIRST, SPI_MODE0);
LED led(LED_BUILTIN);

void clear()
{
  debugLn("clear");
	SPI.beginTransaction(spiSettings);
	digitalWrite(chipSelect, LOW);
	SPI.transfer(0x76);
	digitalWrite(chipSelect, HIGH);
	delay(1);
	SPI.endTransaction();
}

void brightness()
{
  debugLn("brightness");
	SPI.beginTransaction(spiSettings);
	digitalWrite(chipSelect, LOW);
	SPI.transfer(0x7A);
	SPI.transfer(0x1F);
	digitalWrite(chipSelect, HIGH);
	delay(1);
	SPI.endTransaction();
}

void sendString(String value)
{
  debugLn("sendString");
	SPI.beginTransaction(spiSettings);
	digitalWrite(chipSelect, LOW);
	for (uint i = 0; i < value.length(); i++)
	{
		SPI.transfer(value[i]);
	}
	digitalWrite(chipSelect, HIGH);
	delay(1);
	SPI.endTransaction();
}

void setup()
{
	pinMode(chipSelect, OUTPUT);
	digitalWrite(chipSelect, HIGH);

	SPI.begin(); // Initializes the SPI bus

	brightness();
	clear();

	count = 0;

	Serial1.begin(9600); // initialize serial communication at 9600 bits per second:
	debugLn("setup");
}

void loop()
{  
  debugLn("loop");
	// toggle the led
	//led.set(true);

	// Increment the counter
	count = (count + 1) % 10000;
	char str[5];
	snprintf(str, sizeof(str), "%04d", count);

  Serial1.println("Hello, World! From Arduino Nano RP2040 Connect!");
	Serial1.println(str);
	sendString(str);

	// Delay for a bit to make the output readable
	delay(100); // Delay for 100ms
}
