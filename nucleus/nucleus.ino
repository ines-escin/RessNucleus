/*
Code : Measuring oil level in ecopoints with the Nucleus device.
Author : Daniel Ferreira Maida (dfm2@cin.ufpe.br)
Project Leader : Paulo Henrique Monteiro Borba (phmb@cin.ufpe.br)
INES-ESCIN / UFPE.
*/

/*********** LIBRARIES ***********/

#include <Adafruit_CC3000.h>
#include <SPI.h>
#include <Ultrasonic.h>
#include <Adafruit_CC3000.h>
#include <Adafruit_CC3000_Server.h>
#include <ccspi.h>

/*********** CONSTANTS ***********/

#define ADAFRUIT_CC3000_IRQ  3
#define ADAFRUIT_CC3000_VBAT 5
#define ADAFRUIT_CC3000_CS   10
#define pino_trigger 8
#define pino_echo 9
#define pino_trigger2 12
#define pino_echo2 13
#define F2(progmem_ptr) (const __FlashStringHelper *)progmem_ptr
#define IDLE_TIMEOUT_MS  3000
#define WLAN_SSID   "SSID_HERE"  //WLAN SSID
#define WLAN_PASS   "PASSWORD_HERE"   //WLAN PASSWORD
#define WLAN_SECURITY WLAN_SEC_WPA2 //WLAN SECURITY TYPE - WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2

/*********** GLOBAL VARIABLES ***********/

Ultrasonic ultrasonic_sensor(pino_trigger, pino_echo);
Ultrasonic ultrasonic_sensor2(pino_trigger2, pino_echo2);
Adafruit_CC3000 wifiModule = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,SPI_CLOCK_DIV2); 
Adafruit_CC3000_Client fiwareConnection; 
int amountOfLoops = 0;   
const unsigned long dhcpTimeout     = 60L * 1000L;
uint8_t CONNECTION_ATTEMPTS = 3;
uint32_t fiware_ip = wifiModule.IP2U32(000,000,000,000); //FIWARE SERVER IP HERE
uint32_t t = 0L;

/*********** SETUP ***********/

void setup() {
  
  Serial.begin(115200);

  
  Serial.println(F("Initializing WiFi Module..."));
  
  if(!wifiModule.begin())
  {
    Serial.println(F("Fatal error, check your wiring, no shield found!"));
    while(1); // There's nothing we can do in this case :( - loop of death
  }
  connectToAccessPoint();
}

void(* resetFunc) (void) = 0; //This shield can be a little bit unstable, we use the watchdog technic to reset it sometimes.

/*********** MAIN LOOP  ***********/

void loop() {

    amountOfLoops++;
    
    Serial.println(F("Initializing post request...."));
    
    String oilLevel = levelRead();

    if((oilLevel.toInt() <= 100) && (oilLevel.toInt() >= 0))
    {

      Serial.println("Measuring Length");
      int contentLength = 265 + oilLevel.length();

      Serial.println("Connecting to fiware");
      if(!fiwareConnection.connected()){
          connectToFiware();
      }
      
      Serial.println(F("Connected to fiware ..."));
   
      doFiwarePostRequest(oilLevel, contentLength);
    
      Serial.println("Sent...");

      if(fiwareConnection.connected()){
          printResponse();
      }

      Serial.println(F("Stopping connection ..."));
    
      //fiwareConnection.stop();

      delay(100);
      
      Serial.println(F("Closing connection ..."));
    
      fiwareConnection.close();
    
      Serial.println(F("Connection closed ..."));
    }
    else
    {
      Serial.println("Wrong read");
    }
    if(amountOfLoops > 15)
    {
      resetFunc(); 
    }
    Serial.println(amountOfLoops);
    delay(3600 * 1000);
}

/*********** SENSOR FUNCTIONS ***********/

String levelRead()
{
  Serial.println("Reading level");
  long sensorOneLevelValues [10];
  long sensorTwoLevelValues [10];
  
  for(int i = 0; i < 10; i++)
  {
      int echoReturnTime = ultrasonic_sensor.timing();
      long sensorOneDistance = ultrasonic_sensor.convert(echoReturnTime, Ultrasonic::CM);
      sensorOneLevelValues[i] = 100 - (((sensorOneDistance) * 10)/7);
      delay(100);
      
      int echoReturnTime2 = ultrasonic_sensor2.timing();
      long sensorTwoDistance = ultrasonic_sensor2.convert(echoReturnTime2, Ultrasonic::CM);
      sensorTwoLevelValues[i] = 100 - (((sensorTwoDistance) * 10)/7);
      delay(100);
  }

  int modalValue = getModalValue(sensorOneLevelValues, sensorTwoLevelValues);
  Serial.println(modalValue);
  int avgMedianValue = getAverageMedian(sensorOneLevelValues, sensorTwoLevelValues, modalValue);

  return String(avgMedianValue);
}

int getAverageMedian(long sensorOneLevelValues [], long sensorTwoLevelValues [], int modalValue)
{
  Serial.println("Calculating avg");
  int elementCounter = 0;
  int elementSum = 0;
  
  for(int i = 0; i < 10; i++)
  {
    if(((int)sensorOneLevelValues[i]/10) == modalValue)
    {
      elementSum += sensorOneLevelValues[i];
      elementCounter++;
    }
    if(((int)sensorTwoLevelValues[i]/10) == modalValue )
    {
      elementSum += sensorTwoLevelValues[i];
      elementCounter++;
    }
  }

  if(elementCounter != 0)
  {
    return (elementSum/elementCounter);
  }
  else
  {
    return 0;    
  }
}

int getModalValue(long sensorOneLevelValues [], long sensorTwoLevelValues [])
{
  Serial.println("Calculating mode");
  int sensorOneModalValues [11] = {0,0,0,0,0,0,0,0,0,0,0};
  int sensorTwoModalValues [11] = {0,0,0,0,0,0,0,0,0,0,0};
  
  for(int i = 0; i < 11; i++)
  {
    int modalIndex = (int) (sensorOneLevelValues[i]/10);
    sensorOneModalValues[modalIndex] = sensorOneModalValues[modalIndex] + 1;
    
    modalIndex = (int) (sensorTwoLevelValues[i]/10);
    sensorTwoModalValues[modalIndex] = sensorTwoModalValues[modalIndex] + 1;
  }

  int biggestModalValueOne = 0;
  int biggestModalValueTwo = 0;

  int modalValueOne = 0;
  int modalValueTwo = 0;

  for(int i = 0; i < 11; i++)
  {
    if(sensorOneModalValues[i] > biggestModalValueOne)
    {
      biggestModalValueOne = sensorOneModalValues[i];
      modalValueOne = i;
    }
    if(sensorTwoModalValues[i] > biggestModalValueTwo)
    {
      biggestModalValueTwo = sensorTwoModalValues[i];
      modalValueTwo = i;
    }
  }

  if(modalValueOne < modalValueTwo)
  {
    return modalValueOne;
  }
  else
  {
    return modalValueTwo;
  }
}

/*********** CONNECTION FUNCTIONS ***********/

void doFiwarePostRequest(String oilLevel, int contentLength)
{
          
          //headers
          Serial.println("Sent Header 1");
          fiwareConnection.fastrprint(F("POST ")); fiwareConnection.fastrprint(F("/v1/updateContext")); fiwareConnection.fastrprint(F(" HTTP/1.1\r\n")); //post header
          delay(100);
          Serial.println("Sent Header 2");
          fiwareConnection.fastrprint(F("Host: ")); fiwareConnection.fastrprint(F("000.000.000.000:1026")); fiwareConnection.fastrprint(F("\r\n"));   //host header - put your fiware server ip/url here
          Serial.println("Sent Header 3");
          delay(100);
          fiwareConnection.fastrprint(F("Content-Type: application/json")); fiwareConnection.fastrprint(F("\r\n")); // content-type header
          Serial.println("Sent Header 4");
          delay(100);
          fiwareConnection.fastrprint(F("Connection: close")); fiwareConnection.fastrprint(F("\r\n")); // content-type connection
          delay(100);
          Serial.println("Sent Header 5");
          fiwareConnection.fastrprint(F("Content-Length: ")); fiwareConnection.print(contentLength); fiwareConnection.fastrprint(F("\r\n")); //content-length header
          delay(100);
          Serial.println("Sent Header 6");
          fiwareConnection.fastrprint(F("\r\n")); // empty line for body
          //body
          delay(100);
          Serial.println("Sent Body 1");
          fiwareConnection.fastrprint(F("{ \"contextElements\": [ { \"type\": \"Nucleus\", \"isPattern\": \"false\", \"id\": \"NucleusAlpha\", \"attributes\": [ { \"name\": \"coordinates\", \"type\": \"String\", \"value\": \"-8.055404,-34.951013\" }, { \"name\": \"level\", \"type\": \"float\", \"value\": \""));
          delay(100);
          Serial.println("Sent Body 2");
          fiwareConnection.print(oilLevel);
          delay(100);
          Serial.println("Sent Body 3");
          fiwareConnection.fastrprint(F("\" } ] } ], \"updateAction\": \"APPEND\" }"));
          delay(100);
          Serial.println("Sent Body 4");
          fiwareConnection.fastrprintln("");
          delay(100);
          Serial.println(oilLevel);
}

void printResponse()
{
    Serial.println(F("Printing Response..."));
    
    unsigned long lastRead = millis();
    while (fiwareConnection.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
    while (fiwareConnection.available()) {
        char c = fiwareConnection.read();
        Serial.print(c);
        lastRead = millis();
      }
    }
    
}

void connectToFiware()
{
  Serial.println("checking connection");
  if(!wifiModule.checkConnected())
       {
         Serial.println(F("Connection dropped"));
         connectToAccessPoint();
       } 
       else
       {
        delay(100);
        Serial.println("connect to ip");
        fiwareConnection = wifiModule.connectTCP(fiware_ip, 1026);
       }
       Serial.println(F("TCP Connected"));
}

void connectToAccessPoint()
{
  Serial.println(F("\nDeleting old connection profiles"));
  
  while (!wifiModule.deleteProfiles()) {
    Serial.println(F("Failed to delete old profiles!"));
  }
  
  Serial.println(F("Deleted!"));

  Serial.println(F("Establishing connection to the access point"));
  
  while (!wifiModule.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY,CONNECTION_ATTEMPTS)) {
     Serial.println(F("Connection to access point failed...Trying again..."));
     delay(100);
  }
  
  Serial.println(F("Connected!"));

  Serial.print(F("Requesting address from DHCP server..."));
  for(t=millis(); !wifiModule.checkDHCP() && ((millis() - t) < dhcpTimeout); delay(1000));
  if(wifiModule.checkDHCP()) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("failed"));
    return;
  } 
}

