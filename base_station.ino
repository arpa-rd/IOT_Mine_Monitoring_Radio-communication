// ***Base station***

//Server information 
#define BLYNK_TEMPLATE_ID "abcd"
#define BLYNK_TEMPLATE_NAME "Mine Environment"
#define BLYNK_AUTH_TOKEN "kfjghfiussofnsofhsorfhois"
#define BLYNK_PRINT Serial

//libraries
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

//nrf
RF24 radio(4, 5);                 // nRF24L01 (CE,CSN)
RF24Network network(radio);       // Include the radio in the network
const uint16_t base = 00;         // Address of this node in Octal format ( 04,031, etc)
const uint16_t sub_station = 01;  // Address of the other node (substation) in Octal format
const uint16_t worker = 011;      // Address of the other node (worker) in Octal format

//variables and constants
struct MyData {
  float temp;
  float pressure;
  float humidity;
  float gas;
};
float T, P, H, G;
MyData sensorValue;
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "xyz";  // type your wifi name
char pass[] = "12345";  // type your wifi password
BlynkTimer timer;

void sendSensor()
{
  // Please don't send more than 10 values per second.
  //sending real time environmental data recieved from substation to the server
  Blynk.virtualWrite(V0, T);
  Blynk.virtualWrite(V1, P);
  Blynk.virtualWrite(V2, H);
  Blynk.virtualWrite(V3, G);
  //alert if threshold crossed
  if (T > 35)
    Blynk.logEvent("temp_alert", "Temperature is above 35 degree!");
  if (H > 75)
    Blynk.logEvent("humidity_alert", "Humidity is more than 75%!");
  if (G > 180)
    Blynk.logEvent("gas_alert", "Toxic gas in the air!");
}

void setup() {
  Serial.begin(115200); //serial monitor

  //nrf module setup
  SPI.begin();
  radio.begin();
  network.begin(90, base);  //(channel, node address)
  radio.setDataRate(RF24_2MBPS);

  //server setup
  Blynk.begin(auth, ssid, pass);
  timer.setInterval(100L, sendSensor);
}

void loop() {
  //server starting
  Blynk.run();
  timer.run();
  
  network.update();
  //===== "Receiving from substation & worker" =====//
  while (network.available()) {
    RF24NetworkHeader header;
    MyData sensorValue;
    network.read(header, &sensorValue, sizeof(sensorValue)); // Read the incoming data
    if (header.from_node == 011) { // If data comes from Node 011 (worker)
      Blynk.logEvent("worker1_alert", "Worker1 in danger!");
    }
    else {  // If data comes from another node (substation)
      T = sensorValue.temp;
      P = sensorValue.pressure / 1000;
      H = sensorValue.humidity;
      G = sensorValue.gas;
    }
  }
}
