#include <SPI.h>

const int ledPin = 2;
int count = 0;
int baud = 9600;
SPISettings settings = SPISettings(baud, MSBFIRST, SPI_MODE0);

void setup() {
  pinMode(ledPin, OUTPUT);   // initialize digital pin LED_BUILTIN as an output.
  digitalWrite(ledPin, HIGH);  // turn the LED on (HIGH is the voltage level)

  SPI.begin(); // Initializes the SPI bus
  brightness();
  clear();
  sendString("1234");
}

void clear()
{
  SPI.beginTransaction(settings);
  SPI.transfer(0x76);
  SPI.endTransaction();
}

void brightness()
{
  SPI.beginTransaction(settings);
  SPI.transfer(0x7A);
  SPI.transfer(0x1F);
  SPI.endTransaction();
}

void sendString(String value)
{
  SPI.beginTransaction(settings);
  for (int i=0; i<4; i++)
  {
    SPI.transfer(value[i]);
  }
  SPI.endTransaction();
}

void loop() {

  //sendString("1234");

  // toggle the led
  digitalWrite(ledPin, !digitalRead(ledPin));

  // Increment the counter
  count = (count + 1) % 10000;

  // Delay for a bit to make the output readable
  delay(100);  // Delay for 100ms
}
