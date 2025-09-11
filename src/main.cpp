#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

// Defines pin numbers
const int trigPin = 12; // Connects to the Trig pin of the HC-SR04P
const int echoPin = 13; // Connects to the Echo pin of the HC-SR04P

// Defines variables
long duration;  // Variable for the duration of sound wave travel
int distanceCm; // Variable for the distance measurement in centimeters

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
                            // initialize LCD
  lcd.init();
  // turn on LCD backlight
  lcd.backlight();
}

void loop() {

  // Clears the trigPin by setting it LOW for a short period
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Sets the trigPin HIGH for 10 microseconds to send a pulse
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculates the distance in centimeters
  // Speed of sound in air is approximately 343 meters/second or 0.0343
  // cm/microsecond The pulse travels to the object and back, so divide by 2
  distanceCm = duration * 0.0343 / 2;

  // Displays the distance on the Serial Monitor
  lcd.setCursor(0, 0);
  lcd.print("Distance: ");
  lcd.print(distanceCm);
  lcd.print(" cm");

  delay(1000); // Small delay to allow for stable readings
}
