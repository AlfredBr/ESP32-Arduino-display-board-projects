void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);  // turn the LED on (HIGH is the voltage level)
  // Initialize serial communication at 9600 bits per second:
  Serial1.begin(9600);
}

void loop() {
  // Static variable to keep track of the number of messages sent
  static int count = 0;

  if (count <= 0) {
    Serial1.println("BEGIN");
  }

  // Check if the count is less than 10
  if (count < 10) {
    // Send "Hello World!" to the serial port
    Serial1.println("Hello World! From the Arduino Nano RP2040 !!!");
  }
  
  if (count == 10) {
    Serial1.println("DONE");
  }

  // Increment the counter
  count++;

  // Delay for a bit to make the output readable
  delay(1000); // Delay for 1 second
}
