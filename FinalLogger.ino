/*This is the first code for taking data from the 5TM sensor and showing it on the Arduino serial mornitor 
//The data is collected from the SDI12 sensor and timestamped then stored onto an SD card */
#include <SDI12.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "RTClib.h"


#define ECHO_TO_SERIAL   1 // echo data to serial port
#define WAIT_TO_START    0 // Wait for serial input in setup()

#define SYNC_INTERVAL 60000 // mills between calls to flush() - to write data to the card
uint32_t syncTime = 0; // time of last sync()

#define DATAPIN 5         // change to the proper pin for sdi-12 data pin, I prefer D7
SDI12 mySDI12(DATAPIN); 

//Define the number of sensors connected 

RTC_PCF8523 RTC; // define the Real Time Clock object

//the digitalpins that connect to the LEDS
#define redLEDpin 2
#define greenLEDpin 3
unsigned long previousMillis = 0;        
const long interval = 60000;        //set the interval (in milliseconds).  ex: 60000 = 60 seconds

// for the data logging shield, we use digital pin 10 for the SD cs line
const int chipSelect = 10;

// the logging file
File logfile;

//Function for handling errors that occur
void error(char *str)
{
  Serial.print("error: ");
  Serial.println(str);
  
  // red LED indicates error
  digitalWrite(redLEDpin, HIGH);

  while(1);
}

//Function for the working with the real time clock 
//and time stamping the data collected from the sensors
void timestamp(){
  
// fetch the time

  DateTime now = RTC.now();
  // log time
  //logfile.print(now.unixtime()); // seconds since 1/1/1970
  logfile.print(", ");
  logfile.print('"');
  logfile.print(now.year(), DEC);
  logfile.print("/");
  logfile.print(now.month(), DEC);
  logfile.print("/");
  logfile.print(now.day(), DEC);
  logfile.print(" ");
  logfile.print(now.hour(), DEC);
  logfile.print(":");
  logfile.print(now.minute(), DEC);
  logfile.print(":");
  logfile.print(now.second(), DEC);
  logfile.print('"');
#if ECHO_TO_SERIAL
  //Serial.print(now.unixtime()); // seconds since 1/1/1970
  Serial.print(", ");
  Serial.print('"');
  Serial.print(now.year(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);
  Serial.print(":");
  Serial.print(now.second(), DEC);
  Serial.print('"');
#endif //ECHO_TO_SERIAL
  
  }

 

//Function that runs at the start of the program once 
void setup () 
{
  //Debugging leds
     Serial.begin(57600);
     mySDI12.begin();
     pinMode(greenLEDpin, OUTPUT);
     pinMode(redLEDpin, OUTPUT);
     
     #if WAIT_TO_START
      Serial.println("Type any character to start");
      while (!Serial.available());
      #endif //WAIT_TO_START

          // initialize the SD card
      Serial.print("Initializing SD card...");
      // make sure that the default chip select pin is set to
      // output, even if you don't use it:
      pinMode(10, OUTPUT);
      
      // see if the card is present and can be initialized:
      if (!SD.begin(chipSelect)) {
        error("Card failed, or not present");
      }
      Serial.println("card initialized.");
      
      // create a new file
      char filename[] = "LOGGER00.CSV";
      for (uint8_t i = 0; i < 100; i++) {
        filename[6] = i/10 + '0';
        filename[7] = i%10 + '0';
        if (! SD.exists(filename)) {
          // only open a new file if it doesn't exist
          logfile = SD.open(filename, FILE_WRITE); 
          break;  // leave the loop!
        }
      }
      
      if (! logfile) {
        error("couldnt create file");
      }
      
      Serial.print("Logging to: ");
      Serial.println(filename);
    
      // connect to RTC
      Wire.begin();  
      if (!RTC.begin()) {
        logfile.println("RTC failed");
    #if ECHO_TO_SERIAL
        Serial.println("RTC failed");
    #endif  //ECHO_TO_SERIAL
      }

     digitalWrite(greenLEDpin, LOW);    //turn off the onboard LED to start with
     
      
     logfile.println("Sketch for sampling multiple SDI12 sensors");
     logfile.print(", ,");
     for(int i=0; i<=2; i++){
      logfile.print("      Sensor_Num, Ea, VWC(%), Temp(degC)       ");
      logfile.print(",");
      }
      logfile.println();
     #if ECHO_TO_SERIAL
     Serial.println("Sketch for sampling multiple SDI12 sensors");
    for(int i=0; i<=2; i++){
      Serial.print("      Sensor_Num, Ea, VWC(%), Temp(degC)       ");
      }
      #endif//ECHO_TO_SERIAL
      
     Serial.println();
     delay(1000); 
}

void loop () 
{ 
    //unsigned long currentMillis = millis(); 
    uint32_t currentMillis = millis();
    if(currentMillis - previousMillis >= interval || previousMillis == 0) {
   
       previousMillis = currentMillis;   
       digitalWrite(greenLEDpin, HIGH);     //turn on the onboard LED to show that samples are being taken
       Serial.print("Time: ");
       Serial.print(currentMillis/1000);  //print out the current time, in seconds
       logfile.print(currentMillis/1000);
       Serial.print("-----");
       timestamp();
       for (char j = '1'; j <= '3'; j++)  {        //go through channels 1 to 6 
         tmMeasurement(j);
         }
       logfile.println();
       Serial.println(); 
       digitalWrite(greenLEDpin, LOW);     //turn off the LED to show that samples are done
     
     // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
    // which uses a bunch of power and takes time
  if ((millis() - syncTime) < SYNC_INTERVAL) return;
  syncTime = millis();
  
  // blink LED to show we are syncing data to the card & updating FAT!
  digitalWrite(redLEDpin, HIGH);
  logfile.flush();
  digitalWrite(redLEDpin, LOW);
     }
     
     delay(1000);
     
} 

void tmMeasurement(char c){        //5TM soil moisture sensor
  String command = ""; 
  float Ea = 0.0;
  float  temp = 0.0;
  float VWC = 0.0;
  command += c;
  command += "M!"; // SDI-12 measurement command format  [address]['M'][!]
  mySDI12.sendCommand(command); 
  delay(500); // wait a sec
  mySDI12.flush();

  command = "";
  command += c;
  command += "D0!"; // SDI-12 command to get data [address][D][dataOption][!]
  mySDI12.sendCommand(command);
  delay(500); 
  
    if(mySDI12.available() > 0){
        int channel = mySDI12.parseInt();
        Ea = mySDI12.parseFloat();
        temp = mySDI12.parseFloat();

      VWC = (4.3e-6*(Ea*Ea*Ea)) - (5.5e-4*(Ea*Ea)) + (2.92e-2 * Ea) - 5.3e-2 ;     //the TOPP equation used to calculate VWC


      //Serial.print("Sensor:");
      //logfile.print(",");
      logfile.print(",");
      logfile.print(channel);
      logfile.print(",");
      //Serial.print(": Ea=");
      logfile.print(Ea);             //apparent dielectric permittivity
      logfile.print(",");
      //Serial.print(", VWC=");
      logfile.print(VWC);            //volumetric water content 
      logfile.print(",");
      //Serial.print("%, Temp=");
      logfile.print(temp);           //Tempeerature 
      //logfile.print("degC  ....  ");
      
      #if ECHO_TO_SERIAL
      //Serial.print("Sensor:");
      Serial.print(",");
      Serial.print(channel);
      Serial.print(",");
      //Serial.print(": Ea=");
      Serial.print(Ea);             //apparent dielectric permittivity
      Serial.print(",");
      //Serial.print(", VWC=");
      Serial.print(VWC);            //volumetric water content 
      Serial.print(",");
      //Serial.print("%, Temp=");
      Serial.print(temp);           //Tempeerature 
      Serial.print("....  ");
      #endif
   } 
  mySDI12.flush(); 
}
