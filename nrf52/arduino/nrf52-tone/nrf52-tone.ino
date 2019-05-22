#define BUZZER 27

void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  // analogWrite(27, 128);

  HwPWM0.addPin(BUZZER);
  HwPWM0.begin();
  HwPWM0.setResolution(8);
  //HwPWM0.setClockDiv(PWM_PRESCALER_PRESCALER_DIV_32); // freq = 500KHz
  HwPWM0.setClockDiv(PWM_PRESCALER_PRESCALER_DIV_64); // freq = 500KHz
  //HwPWM0.setClockDiv(PWM_PRESCALER_PRESCALER_DIV_128); // freq = 500KHz
  HwPWM0.writePin(BUZZER, 128, false);
}

// the loop function runs over and over again forever
void loop() {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
}
