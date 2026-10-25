unsigned long counter = 1;

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("UART TEST START");
}

void loop() {
  Serial.print("test ");
  Serial.println(counter);

  counter++;
  delay(1000);
}
