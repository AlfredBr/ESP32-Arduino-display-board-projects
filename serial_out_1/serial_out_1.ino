const int ledPin = 2;
int count = 0;

void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);  // turn the LED on (HIGH is the voltage level)
  // Initialize serial communication at 9600 bits per second:
  Serial1.begin(9600);
}

void loop() {
  while (true) {
    // Static variable to keep track of the number of messages sent

    if (count <= 0) {
      Serial1.println("BEGIN");
    }

    // Check if the count is less than 10
    //if (count < 10) {
      // Send "Hello World!" to the serial port
      Serial1.println("Hello World! From the Arduino Nano RP2040 !!!");
    //}

    if (count == 10) {
      Serial1.println("DONE");
    }

    // Increment the counter
    count++;

    // toggle the led
    digitalWrite(ledPin, !digitalRead(ledPin));

    // Delay for a bit to make the output readable
    delay(2000);  // Delay for 1 second
  }
}
