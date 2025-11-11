#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
// LCD Configuration
LiquidCrystal_I2C lcd(0x27, 16, 2);
// DHT Sensor Configuration
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
// GPS Configuration
const int GPS_RX_PIN = 3;
const int GPS_TX_PIN = 4;
SoftwareSerial GPS_SoftSerial(GPS_RX_PIN, GPS_TX_PIN);
TinyGPSPlus gps;
// SIM900 Configuration
const int SIM900_RX_PIN = 12;
const int SIM900_TX_PIN = 13;
SoftwareSerial SIM900(SIM900_RX_PIN, SIM900_TX_PIN);
// Sensor Pins
const int moisturePin = A1;
const int relayPin = 5;
// ThingSpeak Configuration
const char APN[] = "airtelgprs.com";
const char URL[] = "api.thingspeak.com";
const char API_KEY[] = "NOECK9BHN5K3B4N1";
const unsigned long UPLOAD_INTERVAL = 10000; // 5 seconds
unsigned long lastUploadTime = 0;
// Other Constants
const int moistureThreshold = 80;
const int GPS_BAUD_RATE = 9600;
const int SERIAL_BAUD_RATE = 9600;
bool gprsInitialized = false;
void setup() {
// Initialize Serial Communications
Serial.begin(SERIAL_BAUD_RATE);
GPS_SoftSerial.begin(GPS_BAUD_RATE);
SIM900.begin(9600);
// Initialize LCD
lcd.init();
lcd.backlight();
lcd.clear();
// Initialize Sensors and Relay
pinMode(relayPin, OUTPUT);
digitalWrite(relayPin, LOW);
dht.begin();
// Initialize SIM900
Serial.println(F("Initializing SIM900..."));
delay(2000);
if (!resetSIM900()) {
Serial.println(F("SIM900 reset failed"));
}
Serial.println(F("Smart Agriculture System Ready"));
Serial.println(F(" "));
}
void loop() {
// Read Sensors
int moisturePercentage = readMoisture();
float dhtTemp = readTemperature();
float dhtHumidity = readHumidity();
// Update LCD
updateLCD(moisturePercentage, dhtTemp, dhtHumidity);
// Update GPS
updateGPS(1000);
// Print Local Data
printEnvironmentalData(moisturePercentage, dhtTemp, dhtHumidity);
printGPSData();
// Control Relay
controlRelay(moisturePercentage);
// Upload to ThingSpeak if interval has passed
if (millis() - lastUploadTime >= UPLOAD_INTERVAL) {
uploadToThingSpeak(moisturePercentage, dhtTemp, dhtHumidity);
lastUploadTime = millis();
}
Serial.println(F(" "));
delay(1000);
}
float readTemperature() {
float temp = dht.readTemperature();
if (isnan(temp)) {
Serial.println(F("Failed to read temperature"));
return 0.0;
}
return temp;
}
float readHumidity() {
float humidity = dht.readHumidity();
if (isnan(humidity)) {
Serial.println(F("Failed to read humidity"));
return 0.0;
}
return humidity;
}
int readMoisture() {
int moistureValue = analogRead(moisturePin);
return map(moistureValue, 0, 1023, 0, 100);
}
void updateLCD(int moisturePercentage, float dhtTemp, float dhtHumidity) {
lcd.setCursor(0, 0);
lcd.print("T:");
if (dhtTemp == 0) {
lcd.print("--.-");
} else {
lcd.print(dhtTemp, 1);
}
lcd.print("C");
lcd.setCursor(0, 1);
lcd.print("M:");
lcd.print(moisturePercentage);
lcd.print("% ");
lcd.print("H:");
if (dhtHumidity == 0) {
lcd.print("--.-");
} else {
lcd.print(dhtHumidity, 1);
}
lcd.print("%");
}
void updateGPS(unsigned long timeout) {
unsigned long start = millis();
do {
while (GPS_SoftSerial.available()) {
gps.encode(GPS_SoftSerial.read());
}
} while (millis() - start < timeout);
}
void printEnvironmentalData(int moisturePercentage, float dhtTemp, float dhtHumidity) {
Serial.println(F("\nEnvironmental Data:"));
Serial.print(F("Soil Moisture: "));
Serial.print(moisturePercentage);
Serial.println(F("%"));
Serial.print(F("DHT Temperature: "));
if (dhtTemp == 0) {
Serial.println(F("No reading"));
} else {
Serial.print(dhtTemp, 1);
Serial.println(F(" C"));
}
Serial.print(F("DHT Humidity: "));
if (dhtHumidity == 0) {
Serial.println(F("No reading"));
} else {
Serial.print(dhtHumidity, 1);
Serial.println(F("%"));
}
}
void printGPSData() {
Serial.println(F("\nGPS Data:"));
if (gps.location.isValid()) {
Serial.print(F("Latitude: "));
Serial.println(gps.location.lat(), 6);
Serial.print(F("Longitude: "));
Serial.println(gps.location.lng(), 6);
Serial.print(F("Altitude: "));
if (gps.altitude.isValid()) {
Serial.print(gps.altitude.meters());
Serial.println(F(" meters"));
} else {
Serial.println(F("Invalid"));
}
} else {
Serial.println(F("GPS Signal Not Available"));
}
}
void controlRelay(int moisturePercentage) {
if (moisturePercentage < moistureThreshold) {
Serial.println(F("\nMoisture below threshold - Relay ON"));
digitalWrite(relayPin, HIGH);
} else {
Serial.println(F("\nMoisture above threshold - Relay OFF"));
digitalWrite(relayPin, LOW);
}
}
bool uploadToThingSpeak(int moisture, float temp, float humidity) {
if (!gprsInitialized) {
if (!initializeGPRS()) {
Serial.println(F("GPRS initialization failed"));
return false;
}
gprsInitialized = true;
}
String dataString = String("/update?api_key=") + API_KEY;
dataString += "&field1=" + String(temp, 1);
dataString += "&field2=" + String(humidity, 1);
dataString += "&field3=" + String(moisture);
if (gps.location.isValid()) {
dataString += "&field4=" + String(gps.location.lat(), 6);
dataString += "&field5=" + String(gps.location.lng(), 6);
}
return sendDataToThingSpeak(dataString);
}
bool resetSIM900() {
return (sendCommand("AT", "OK", 2000) &&
sendCommand("AT+CFUN=1", "OK", 10000));
}
bool initializeGPRS() {
if (!sendCommand("ATE0", "OK", 2000)) return false;
if (!sendCommand("AT+CMEE=2", "OK", 2000)) return false;
if (!sendCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK", 2000)) ;
if (!sendCommand("AT+SAPBR=3,1,\"APN\",\"" + String(APN) + "\"", "OK", 2000)) ;
if (!sendCommand("AT+SAPBR=1,1", "OK", 30000)) ;
if (!sendCommand("AT+SAPBR=2,1", "+SAPBR: 1,1", 2000)) {
sendCommand("AT+SAPBR=0,1", "OK", 2000);
return false;
}
return true;
}
bool sendDataToThingSpeak(String dataString) {
bool success = false;
if (!sendCommand("AT+HTTPINIT", "OK", 2000)) goto cleanup;
if (!sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000)) goto cleanup;
if (!sendCommand("AT+HTTPPARA=\"URL\",\"" + String(URL) + dataString + "\"", "OK", 2000))
goto cleanup;
if (!sendCommand("AT+HTTPACTION=0", "+HTTPACTION: 0,200", 30000)) goto cleanup;
if (!sendCommand("AT+HTTPREAD", "OK", 8000)) goto cleanup;
success = true;
Serial.println(F("Data uploaded to ThingSpeak successfully"));
cleanup:
sendCommand("AT+HTTPTERM", "OK", 2000);
if (!success) {
gprsInitialized = false;
Serial.println(F("Failed to upload data to ThingSpeak"));
}
return success;
}
bool sendCommand(String command, String expectedResponse, unsigned long timeout) {
Serial.println("> " + command);
SIM900.println(command);
String response = "";
unsigned long startTime = millis();
while (millis() - startTime < timeout) {
while (SIM900.available()) {
char c = SIM900.read();
response += c;
if (response.indexOf("ERROR") != -1 || response.indexOf("FAIL") != -1) {
Serial.println("< " + response);
return false;
}
if (response.indexOf(expectedResponse) != -1) {
Serial.println("< " + response);
return true;
}
}
}
Serial.println("< TIMEOUT: " +
response); return false;
}
