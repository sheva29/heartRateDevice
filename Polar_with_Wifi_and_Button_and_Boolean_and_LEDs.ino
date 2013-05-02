#include <HttpClient.h>
#include <Cosm.h>
#include <WiFlyHQ.h>
#include <SoftwareSerial.h>
#include "Wire.h"
#define HRMI_I2C_ADDR      127
#define HRMI_HR_ALG        1

SoftwareSerial wifiSerial(8,9);
WiFly wifly;

const char mySSID[] =  "internetz"; //"SBG658060"; 
const char myPassword[] =  "1nt3rn3tz";//"SBG6580E58E60";  
char site[] = "api.cosm.com";
char feedId[] = "104810"; //FEED ID
char cosmKey[] = "ewTCG0qri8i6jXsXxwXxnrAZpnKSAKxHL0tnbndNeEpPdz0g";
char sensorId[] = "Mauricio"; 

const int buttonPin = A2; //  When pushed starts to send data to COSM
const int dataLEDPin = 8; // When the button is pressed it indicates that data is bieng pushed to COSM
const int internetLEDPin = 9; // Indicates if the device is connected to the internet
const int sensorLEDPin = 10; // Indicates if the HRMI is receving data from the strap
int previousButtonState = HIGH; 
boolean sendData = false;
long currentMillis = 0;
long lastMillis = 0;
long sensorCheckMillis = 0;

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(dataLEDPin, OUTPUT);
  pinMode(internetLEDPin, OUTPUT);
  pinMode(sensorLEDPin, OUTPUT);
  digitalWrite(dataLEDPin, LOW);
  digitalWrite(internetLEDPin, LOW);
  digitalWrite(sensorLEDPin, LOW);        
  Keyboard.begin();

  setupHeartMonitor(HRMI_HR_ALG);
  Serial.begin(115200);
  Serial.println("Starting PULSE upload to Cosm...");

  char buf[32];

  Serial.println("Starting");
  Serial.print("Free memory: ");
  Serial.println(wifly.getFreeMemory(),DEC);
  Serial1.begin(9600); //THIS IS ONLY AVAILABLE ON LEONARDO
  if (!wifly.begin(&Serial1, &Serial)) {
    Serial.println("Failed to start wifly");
  }

  if (!wifly.isAssociated()) {
    Serial.println("Joining network");
    wifly.setSSID(mySSID);
    wifly.setPassphrase(myPassword);
    wifly.enableDHCP();
    if (wifly.join()) {
      Serial.println("Joined wifi network");
    } 
    else {
      Serial.println("Failed to join wifi network");    
    }
  } 
  else {
    Serial.println("Already joined network");
  }

  Serial.print("MAC: ");
  Serial.println(wifly.getMAC(buf, sizeof(buf)));
  Serial.print("IP: ");
  Serial.println(wifly.getIP(buf, sizeof(buf)));
  Serial.print("Netmask: ");
  Serial.println(wifly.getNetmask(buf, sizeof(buf)));
  Serial.print("Gateway: ");
  Serial.println(wifly.getGateway(buf, sizeof(buf)));
  wifly.setDeviceID("Wifly-WebClient");
  Serial.print("DeviceID: ");
  Serial.println(wifly.getDeviceID(buf, sizeof(buf)));

  if (wifly.isConnected()) {
    Serial.println("Old connection active. Closing");
    wifly.close();
  }

  if (wifly.open(site, 8081)) {
    Serial.print("Connected to ");
    Serial.println(site);
    
  } 
  else {
    Serial.println("Failed to connect");
  }
  Serial.println("Set up Complete");
  
}




void loop() {
  
  //Add the state of the button
  currentMillis = millis();
  int buttonState = digitalRead(buttonPin);
  //If the button is push it sends data to COSM
  if ((buttonState != previousButtonState) 
    && (buttonState == HIGH)) {
    Keyboard.print(" ");
    sendData = !sendData;
  }
  previousButtonState = buttonState; 

  int heartRate = getHeartRate();
  
  //Checks if connected to the internet the LED will light up
  if(wifly.isConnected()){
    digitalWrite(internetLEDPin, HIGH);
  }
  // When connecting to the internet if connection established internetLED will light up
  if(!wifly.isConnected()){
    if (wifly.open(site, 8081)) {
      Serial.print("Connected to ");
      Serial.println(site);
      digitalWrite(internetLEDPin, HIGH);
    } 
    //Otherwise if connection is not established it goes off
    else {
      Serial.println("Failed to connect");
      digitalWrite(internetLEDPin, LOW);
    }
  }
  // If 5 seconds have passed and 
  if(currentMillis - sensorCheckMillis >= 5000){
    
    //If the heart rate coming from the HRMI is higher than 0 Purple LED lights up
    if(heartRate > 0){
      digitalWrite(sensorLEDPin, HIGH);
    }
    //Otherwise is off
    else{
      digitalWrite(sensorLEDPin, LOW);
    }
    sensorCheckMillis = millis();
  }
  //Makes sure to send data only every second instead of every heartbeat
  if(sendData){
    if(currentMillis - lastMillis >= 1000){
      sendDataToCosm(heartRate);
      Serial.println(heartRate);
      lastMillis = millis();
    }
    digitalWrite(dataLEDPin, HIGH);
  }
  else{
    digitalWrite(dataLEDPin, LOW); 
  }

  delay(10);
}

//Sends data to COSM through a socket connection
void sendDataToCosm(int data){
  wifly.print("{\"method\" : \"put\",");
  wifly.print("\"resource\" : \"/feeds/");
  wifly.print(feedId);
  wifly.print("\", \"headers\" :{\"X-ApiKey\" : \"");
  wifly.print(cosmKey);
  wifly.print("\"},\"body\" :{ \"version\" : \"1.0.0\",\"datastreams\" : [{\"id\" : \"");
  wifly.print(sensorId);
  wifly.print("\",\"current_value\" : \"");
  wifly.print(data);
  wifly.print("\"}]}}");
  wifly.println();
}

void setupHeartMonitor(int type){
  //setup the heartrate monitor
  Wire.begin();
  writeRegister(HRMI_I2C_ADDR, 0x53, type); // Configure the HRMI with the requested algorithm mode
}

int getHeartRate(){
  byte i2cRspArray[3]; // I2C response array
  i2cRspArray[2] = 0;

  writeRegister(HRMI_I2C_ADDR,  0x47, 0x1); // Request a set of heart rate values 

  if (hrmiGetData(127, 3, i2cRspArray)) {
    return i2cRspArray[2];
  }
  else{
    return 0;
  }
}

void writeRegister(int deviceAddress, byte address, byte val) {
  Wire.beginTransmission(deviceAddress); // start transmission to device 
  Wire.write(address);       // send register address
  Wire.write(val);         // send value to write
  Wire.endTransmission();     // end transmission
}

boolean hrmiGetData(byte addr, byte numBytes, byte* dataArray){
  Wire.requestFrom(addr, numBytes);
  if (Wire.available()) {
    for (int i=0; i<numBytes; i++){
      dataArray[i] = Wire.read();
    }
    return true;
  }
  else{
    return false;
  }
}




