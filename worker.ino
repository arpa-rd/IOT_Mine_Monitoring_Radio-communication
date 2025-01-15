// worker station

//libraries
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

MAX30105 particleSensor;

//nrf
RF24 radio(8, 10);               // nRF24L01 (CE,CSN)
RF24Network network(radio);      // Include the radio in the network
const uint16_t worker = 011;     // Address of our node in Octal format ( 04,031, etc)
const uint16_t sub_station = 01; // Address of the other node (sub station) in Octal format
const uint16_t base = 00;        // Address of the other node (base station) in Octal format

//variables and constants
#define led 5
#define buzzer 4
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
float beatsPerMinute;
int beatAvg;
long bpm_start;

void setup() {
  Serial.begin(115200);  //serial monitor

  pinMode(led, OUTPUT);
  pinMode(buzzer, OUTPUT);

  //nrf module setup
  SPI.begin();
  radio.begin();
  network.begin(90, worker);  //(channel, node address)
  radio.setDataRate(RF24_2MBPS);

  //MAX30105 sensor setup
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");
  bpm_start = millis();  //sensor starts taking data
  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
}

void loop() {
  int en = 0, c = 0;
  long bpm_end;
  const char text2[] = "Threshold Crossed for worker1";
  
  network.update();
  //===== "Receiving from substation" =====//
  while ( network.available() ) {     // Is there any incoming data?
    RF24NetworkHeader header5;
    char text[32];
    network.read(header5, &text, sizeof(text)); // Read the incoming data
    if (header5.from_node == 01) {    // If data comes from Node 01 (substation)
      digitalWrite(led, HIGH);  //Alert worker for hazardous environment
      tone(buzzer, 1000);
      delay(1000);
      digitalWrite(led, LOW);
      noTone(buzzer);
    }
  }

  //Calculate beats per minute  
  long irValue = particleSensor.getIR();
  if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);
    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable
      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }
  bpm_end = millis();
  
  if ((bpm_end - bpm_start) >= 30000)  //30s time needed to stabilize sensor output
    en = 1; //now sensor is stable and enabled

  if (irValue < 50000)
    beatAvg = 50; //if finger not placed

  //threshold crossed: more than 100bpm or less than 60 (modelled for resting period, can be changed according to working load)
  if (en) {
    if (beatAvg > 100 || beatAvg < 60) {
      //===== "Sending to substation" =====//
      RF24NetworkHeader header6(sub_station);
      bool ok = network.write(header6, &text2, sizeof(text2)); // Send the data

      //===== "Sending to base station" =====//
      RF24NetworkHeader header7(base);
      bool ok2 = network.write(header7, &text2, sizeof(text2)); // Send the data

      //alert worker himself about his health
      digitalWrite(led, HIGH);
      tone(buzzer, 1000);
      delay(1000);
      digitalWrite(led, LOW);
      noTone(buzzer);
    }
  }
}
