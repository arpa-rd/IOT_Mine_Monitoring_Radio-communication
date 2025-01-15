//***Sub station***

//libraries
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <LiquidCrystal_I2C.h>
#include <dht.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);  // Set the LCD address to 0x27 for a 16 chars and 2 line display
Adafruit_BMP280 bmp; 
dht DHT;

//nrf
RF24 radio(8, 10);               // nRF24L01 (CE,CSN)
RF24Network network(radio);      // Include the radio in the network
const uint16_t sub_station = 01;  // Address of our node in Octal format (01, 02, etc)
const uint16_t base = 00;         // Address of the other node (base station) in Octal format
const uint16_t worker = 011;      // Address of the other node (worker) in Octal format

//variables and constants
#define LED 5
#define buzzer 6
#define dhtPin 4
int c = 0;
struct MyData {
  float temp;
  float pressure;
  float hum;
  float gas;
};

void setup() {
  Serial.begin(115200);  //serial monitor

  pinMode(LED, OUTPUT);
  pinMode(buzzer, OUTPUT);

  //nrf module setup
  SPI.begin();
  radio.begin();
  network.begin(90, sub_station); //(channel, node address)
  radio.setDataRate(RF24_2MBPS);

  //LCD Display
  lcd.begin();
  lcd.backlight();

  while (!Serial)
    delay(100); // wait for native usb
  Serial.println(F("BMP280 and MQ135 test"));

  //BMP280 setup
  unsigned status;
  status = bmp.begin(0x76);
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                     "try a different address!"));
    Serial.print("SensorID was: 0x");
    Serial.println(bmp.sensorID(), 16);
    Serial.print("ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("ID of 0x60 represents a BME 280.\n");
    Serial.print("ID of 0x61 represents a BME 680.\n");
    while (1)
      delay(10);
  }
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

}

void loop() {
  long lcd_start, lcd_end, dt;
  network.update();

  //===== "Receiving from worker" =====//
  while ( network.available() ) {     // Is there any incoming data?
    RF24NetworkHeader header3;
    char incomingData[32];
    network.read(header3, &incomingData, sizeof(incomingData)); // Read the incoming data
    if (header3.from_node == 011) {    // If data comes from Node 011 (worker)
      Serial.print("Data from worker: ");
      Serial.println(incomingData);  // print the data
      //for our budget modelling, we didn't keep any monitor at substation for workers information.
      //In real life application, there must be a server for workers health monitoring where the alert will be sent to.
    }
  }

  //read environmental data from sensors
  int readData = DHT.read11(dhtPin);
  MyData sensorValue;
  sensorValue.temp = bmp.readTemperature();
  sensorValue.pressure = bmp.readPressure();
  sensorValue.hum = DHT.humidity;
  sensorValue.gas = analogRead(0);

  //print real time environmental data on lcd display
  if (c == 0) {
    lcd_start = millis();
    c = c + 1;
  }
  lcd_end = millis();
  dt = lcd_end - lcd_start;
  if (dt > 2000) {  //update data every 2s
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Temp(*C)="));
    lcd.print(bmp.readTemperature());
    lcd.setCursor(0, 1);
    lcd.print(F("Pres(Pa)="));
    lcd.print(bmp.readPressure());
    lcd.setCursor(0, 2);
    lcd.print(F("Hum(%)="));
    lcd.print(DHT.humidity);
    lcd.setCursor(0, 3);
    lcd.print("Air=");
    lcd.print(sensorValue.gas);
    lcd_start = millis();
  }

  //===== Sending to base station =====//
  RF24NetworkHeader header4(base); 
  bool ok = network.write(header4, &sensorValue, sizeof(sensorValue)); //send real time data to base

  // Check if threshold crossed
  if (sensorValue.gas > 200 || sensorValue.temp > 50 || sensorValue.hum > 75) {
    //Sound and light alert for the whole substation
    digitalWrite(LED, HIGH);
    tone(buzzer, 1000);
    delay(1000);
    digitalWrite(LED, LOW);
    noTone(buzzer);
    //===== "Sending to worker" =====//
    RF24NetworkHeader header4(worker); //alert workers about substation environment
    char text2[] = "Threshold crossed sub";
    bool ok2 = network.write(header4, &text2, sizeof(text2));
  }
}
