#define BUFFER 1024 // size of the buffer in bytes 
#define SSID   "TesteArduino" 
#define PASS   "12345678"
#define DST_IP "172.20.26.78" //my web site, replace with yours
#define pino_trigger 10
#define pino_echo 11
#define pino_trigger2 12
#define pino_echo2 13

char buffer[BUFFER];  // buffer for returned messages
String cmd;   // AT command string

#define DEBUG Serial      // Send debug messages to serial monitor

char reply[500]; // you wouldn't normally do this
const bool printReply = true;
const char line[] = "-----\n\r";
#include <AltSoftSerial.h>
#include <Ultrasonic.h>

AltSoftSerial ESP8266(8,9);
Ultrasonic ultrasonic_sensor(pino_trigger, pino_echo);
Ultrasonic ultrasonic_sensor2(pino_trigger2, pino_echo2);

// By default we expect OK\r\n back
char OK[] = "OK\r\n";

//=====================================================================
void setup() {
  delay(1000);
  ESP8266.begin(9600); // Start ESP8266 comms 
  Serial.begin(9600); // Start seriam monitor comms for debug messages
  DEBUG.println("Initialising...");
  initESP8266(); // Initialise the ESP8266 module
}

//=====================================================================
String waitForResponse(int wait)
{
    int tempPos = 0;
    long int time = millis();
    while( (time + wait) > millis())
    {
        while(ESP8266.available())
        {
            char c = ESP8266.read(); 
            if (tempPos < 500) { reply[tempPos] = c; tempPos++;   }
        }
        reply[tempPos] = 0;
    } 
 
    if (printReply) { Serial.println( reply );  Serial.println(line);     
    }

    return reply;
}

//=====================================================================
void initESP8266() { 
 // ESP8266.println("AT+RST"); // Reset the ESP8266
 // waitForResponse(10000);
   
  ESP8266.println("AT+CWMODE=1"); // Set Mode 1 (STA = Client)
  waitForResponse(10000);  

  connectToAP();
  
  ESP8266.println("AT+CIPMUX=0"); // Single connection
  waitForResponse(5000); 
  
  ESP8266.println("AT+CIPMODE=0"); // Normal mode
  waitForResponse(5000);  
  
  ESP8266.println("AT+CIFSR"); // Print the IP address of module
  waitForResponse(5000);
  
  DEBUG.println("============================");
}

void connectToAP()
{
  bool isConnected = false;
  while(!isConnected)
  { 
    cmd = "AT+CWJAP=\""; // Join the Access Point
    cmd += SSID;
    cmd += "\",\"";
    cmd += PASS;
    cmd += "\"";
    ESP8266.println(cmd);  //send command to device
    
    String reply = waitForResponse(10000);
    
    if(reply.indexOf("OK") >= 0)
    {
      isConnected = true;
    }
  } 
}

//=====================================================================
void loop() 
{ 
  String level = levelRead();
  Serial.println(level);
  sendLevel(level);
  delay(60000); 
}

void sendLevel(String value) {
  
  cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += DST_IP;
  cmd += "\",1026";
  ESP8266.println(cmd);  
  waitForResponse(10000);

  String cmdBody = "{\"attributes\": [{\"name\":\"level\",\"type\":\"float\",\"value\":\"";
  cmdBody += value;
  cmdBody += "\"}]}";

  Serial.println(cmdBody);
 
  cmd =  "POST /v1/contextEntities/NucleusAlpha/attributes HTTP/1.1\r\n";
  cmd += "Host: 172.20.26.78:1026\r\n"; 
  cmd += "Content-Type: application/json\r\n"; 
  cmd += "Content-length: ";
  cmd +=  String(cmdBody.length());
  cmd += "\r\n\r\n";
  cmd += cmdBody;
 
     
  ESP8266.print("AT+CIPSEND=");
  ESP8266.println(cmd.length());   
  waitForResponse(10000);
  ESP8266.println(cmd);
  waitForResponse(30000); 
  ESP8266.println("AT+CIPCLOSE"); 
  delay(5000); 
  
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
