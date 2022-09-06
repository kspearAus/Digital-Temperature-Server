String fname = "Garage7";   // pin change for pcb
// light sensor control of EPD display
#include "WiFi.h"
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <FS.h>
#include "SPIFFS.h"
#define  FORMAT_SPIFFS_IF_FAILED true
#define  U_PART U_SPIFFS
#include "ota.h"
#include "tools.h"
#include "SDTools.h"
#include <Wire.h>
#include <rom/rtc.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

AsyncWebServer server(80);
  
IPAddress local_IP(192, 168, 86, 27); // static IP address
IPAddress gateway(192, 168, 86, 1);  //gateway static IP address
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
const char* host = "esp32";
const char* ssid = "your wifi name";
const char* password = "your wifi password";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800;
const int   daylightOffset_sec = 3600;
const char* Timezone = "AEST-10AEDT,M10.1.0,M4.1.0/3";   // Australia
 
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 15  // Ocean Controls sensor cable Red Data wire to GPIO4, GND White, VCC Green
// Resistor explained https://electronics.stackexchange.com/questions/62092/1-wire-and-the-resistor
#define FAN_RELAY   14  //GPIO used/set by logic (high=3.3v low=0v)
#define TOUCH_PIN   27  // Screen wake on touch GPIO27 aka T7
#define RECORD_TIME 10  // number of minutes between data recordings
#define SCRN_OFF    20  // Seconds until screen turned off to conserve pixels
#define TOUCH_ON    40  // Capacitance threshold
#define WRITE_TEMP  30
////////////////////////////////
//  NOTE:  Fan turns on when temp is greater than triggerOn value 
//         Fan turns off when temp is less than triggerOff value
double triggerOn  = 30;
double triggerOff = 28; //  Make triggerOff 2c lower than triggerOn to reduce on/off cycles
///////////////////////////////
#include <GxEPD2_BW.h>               //C:\Users\Pip\Documents\Arduino\libraries\Adafruit_GFX_Library\Fonts
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

OneWire oneWire(ONE_WIRE_BUS); // OneWire instance 
DallasTemperature sensors(&oneWire);  // Pass our oneWire reference to Dallas Temperature.

float temp1, temp2 ;
String alltemp, fan, screen, TempData;
int FanOn = 0;
int FanSwitch = 0;
int rt = (RECORD_TIME * 60000);
int tpval;
int clk  = 0; // is screen alive counter (0-99)
int dif = 0; // millis time difference
struct tm timeinfo;
int dayno;
String iip;  int rss;
unsigned long startm, nowm ;
const char* PARAM = "file";
int scrn = 1;  //screen sleep/wake indicator
char daynum[3], timeT[9], timeD[9];

// Addresses of DS18B20s  Use 1wireScan on new device for address check resistor required for any cable changes

//
uint8_t t1[8] = { 0x28, 0xFF, 0x64, 0x1E, 0x15, 0xB1, 0xFA, 0xD8 };  //onboard
uint8_t t2[8] = { 0x28, 0xFF, 0x1F, 0xAE, 0x5D, 0x61, 0xF6, 0xBB };  //ceiling



// light sensor
  int aisVal  = 0;
// photo sensor function esp32 on A3
  #define LightPin 36 /* ESP8266 Analog Pin ADC0 = A0 */   
  int adcValue = 0;  /* Variable to store Output of ADC */ 
  int ledset;        // led brightness 0-15

// breadboard gnd Brown, blue 23, yellow 18, orange 5, gray 3.3v, white 16, purple 4, green 0
// new    19 blank, 23-MISO/DIN-blue, 18-SCK-Yellow, 5-CS-Orange, gray 3v, -- 16-RST-white, 4-Busy-purple, MOSI/DC-Green 0, brown Gnd   
//1_54 epd
GxEPD2_BW<GxEPD2_154_D67,    GxEPD2_154_D67::HEIGHT>    display(GxEPD2_154_D67(/*CS*/ 5, /*DC*/ 0, /*RST*/ 16, /*BUSY*/ 4));
//2_9 epd
//GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> display(GxEPD2_290_T94_V2(/*CS*/ 5, /*DC*/ 17, /*RST*/ 16, /*BUSY*/ 0));
// 5v-gray, GND-brown, 23-MISO/DIN-blue, 17-MOSI/DC-Green, 18-SCK-Yellow, 5-CS-Orange, 16-RST-white, 0/Busy-purple (change;1-wire conflict)

// pull data into string for html
String readtemp1()     {return String(temp1,0);      } 
String readtemp2()     {return String(temp2,0);      } 
String readalltemp()   {return String(alltemp);      } 
String readRSS()       {return String(rss);          }

String processor_update(const String& var) {
  Serial.println(var);
  if (var == "list") {return filelist;  }
  return String();   }
  //display.setFont(&FreeMonoBold9pt7b);
  //display.setFont(&FreeMonoBold12pt7b);
  //display.setFont(&FreeMonoBold18pt7b);


// 1_54 version
void epdDisplay() {
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do   {  display.fillScreen(GxEPD_WHITE);  }
  while (display.nextPage());
  do  {
   //gap, 15 is single space, 17 is a bit more, 20 is 1.5 spaces
   display.setFont(&FreeMonoBold18pt7b);
   display.setCursor(10,  30); display.print("Sensors'");
   display.setFont(&FreeMonoBold12pt7b);
   display.setCursor(10,  90); display.print("Cable");
   display.setCursor(10,  130); display.print("Int");
   display.setFont(&FreeMonoBold9pt7b);
   display.setCursor(10,  190); display.print(  String(fname));
   }
  while (display.nextPage());
  }  // end epdDisplay

 void screenUpdate()  {  //1in54
    display.setRotation(1); //(1in54in is 200x200)
    // Set partial window and WhiteRectangle above font pitch from text setCursor
    //display.setPartialWindow(x, y, w, h);
    display.setPartialWindow(110,40, 90, 150);  //start 11 above text line (pitch + 2)
    display.firstPage();
    do  { display.fillRect(110, 40, 90, 150, GxEPD_WHITE);
     display.setFont(&FreeMonoBold18pt7b);
     display.setCursor(130,  90);  display.print(String(temp1,0));
     display.setCursor(130, 130);  display.print(String(temp2,0));
     display.setFont(&FreeMonoBold12pt7b);
     display.setCursor(130,160);   display.print(String(timeT));
     Serial.println(String(timeT));
     display.setCursor(130,190);   display.print(clk);
        }
    while (display.nextPage());
    }  //end dataUpdate

void lightset() {   // photo sensor not used as only one adc on this board
   adcValue = analogRead(LightPin); /* Read the Analog Input value */
   Serial.print("  PhotoADC: "); Serial.println(adcValue); 
   //Serial.print(" Ledset: "); Serial.print(ledset);  Serial.print(" ");
}
  
void setup() {          //***************  S E T U P  *****************
  Serial.begin(115200);
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  display.init();
  pinMode(LightPin, INPUT);  // light sensor
  
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do  {
   display.fillScreen(GxEPD_WHITE);
  }
  while (display.nextPage());
  
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;  }
    delay(500);
  Serial.println("");
  listDir(SPIFFS, "/", 0);

// set ip address
   if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
  Serial.println("STA Failed to configure"); }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf("WiFi Failed!\n");
        return;    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    iip = WiFi.localIP().toString();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setenv("TZ", Timezone, 1);
  Serial.print("NPT time: ");

  getLocalTime(&timeinfo);
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
   Serial.println("Disconnect from NPTime Server and reset connection");
  //  ** disconnect NPT WiFi connection
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(500);
  // Connect to your wifi for OTA and Web Data server
  WiFi.mode(WIFI_STA); //Connect to your wifi
  WiFi.begin(ssid, password);
  Serial.print("Re-Connecting for esp32Server functions to ");
  Serial.println(ssid);
  //Wait for WiFi to connect
  while(WiFi.waitForConnectResult() != WL_CONNECTED){  Serial.print(".");    }  // display .'s until connected
  Serial.print("Connected to "); Serial.println(ssid); Serial.print("IP: ");   Serial.println(WiFi.localIP());  


  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);  }    }
  Serial.println("mDNS responder started");

  // *****   OTA server pages  *****
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);   });
  server.on("/filesystem", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", FS_HTML, processor_update);  });
  server.on("/filelist", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", filelist.c_str());   });
  server.on("/testpage", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/testpage.html", String(), false);   });
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "Device will reboot in 2 seconds");
    delay(1000);  ESP.restart();    });
  server.on("/doUpdate", HTTP_POST,
  [](AsyncWebServerRequest * request) {},
  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t *data, size_t len, bool final) {
    handleDoUpdate(request, filename, index, data, len, final);    });
  server.on("/doUpload", HTTP_POST, [](AsyncWebServerRequest * request) { opened = false;   },
  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t *data, size_t len, bool final) {
    handleDoUpload(request, filename, index, data, len, final);   });
  server.on("/delete", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/update?state=<inputMessage>
    if (request->hasParam(PARAM)) {
      inputMessage = request->getParam(PARAM)->value();
      inputParam = PARAM;
      deleteFile(SPIFFS, inputMessage);
      Serial.println("-inputMessage-");
      Serial.print("File=");
      Serial.println(inputMessage);
      Serial.println(" has been deleted");    }
   else { inputMessage = "No message sent";
      inputParam = "none";   }
      request->send(200, "text/plain", "OK");  });
  // end ota pages


  // *****   data pages  *****
    server.on("/temp1", HTTP_GET, [](AsyncWebServerRequest *request){ 
      request->send_P(200, "text/plain", readtemp1().c_str());   });
    server.on("/temp2", HTTP_GET, [](AsyncWebServerRequest *request){ 
      request->send_P(200, "text/plain", readtemp2().c_str());   });
    server.on("/alltemp", HTTP_GET, [](AsyncWebServerRequest *request){ 
      request->send_P(200, "text/plain", readalltemp().c_str());   });
     server.on("/rss",   HTTP_GET, [](AsyncWebServerRequest *request){ 
      request->send_P(200, "text/plain", readRSS().c_str());   });
 
  server.onNotFound(notFound);
  server.begin();
  Update.onProgress(printProgress);
  
  epdDisplay();  // set base screen text
  startm = millis();  // timer using system clock
 
  pinMode(FAN_RELAY, OUTPUT);
  }  // end setup       


  void dataWrite() {
    struct tm timeinfo;
           if(!getLocalTime(&timeinfo)){
            Serial.println("Failed to obtain time");
            return;  }

       String dStamp; 
       strftime(daynum, 9, "%d", &timeinfo);
       String daynums = daynum;
       dayno = daynums.toInt();
       strftime(timeT, 9, "%H:%M", &timeinfo);
       strftime(timeD, 9, "%x", &timeinfo);
       dStamp = dStamp + timeD + ",";
       dStamp = dStamp + timeT ;
       dStamp = dStamp + ", ";
       TempData = (String(dStamp) + String(temp1,0) + ", " + String(temp2,0) + "\r\n");
       Serial.println("4PTele " + String(TempData));

       File file = SPIFFS.open("/Garage.txt", "a");
         if(!file){     // File not found 
         writeFile(SPIFFS, "/Garage.txt", "Created ");
         file.print(TempData);
         file.close();     } 
         else { 
           file.print(TempData);  file.close();

           }
           Serial.print("To SPIFFS " + String(TempData));
           Serial.println("Fanswitch " + String(FanSwitch));
       }

void getTemps (){
  rss = WiFi.RSSI();
  rss = (100 + rss);  // signal strength as percentage
  sensors.requestTemperatures();
  if (sensors.getTempC(t1) > -10) temp1 = sensors.getTempC(t1);  else  temp1 = 0;
  if (sensors.getTempC(t2) > -10) temp2 = sensors.getTempC(t2);  else  temp2 = 0;

  // String alltemp used to collect temp data for html page
  alltemp = ("shelf "                    + String(temp1,0)     );
  alltemp = (alltemp + "c  ceiling "   + String(temp2,0)    + "c" );  

  Serial.print("t1 "      + String(temp1,1) + "  t2 "    + String(temp2,1) + "  clk " + String(clk) );  
  Serial.println();
  }

  
void loop(void)    {  //  **********  L O O P  *************
  clk++;   if (clk > 99) clk = 0;  // still alive feature
  getLocalTime(&timeinfo);
  getTemps ();


  nowm = millis();
  dif = (nowm - startm);
  Serial.println("dif " + String(dif));
  lightset();
   if (adcValue < 2000) {                  // it's dark, go to sleep
    scrn = 0;
    Serial.println("Screen hibernate please ");
    display.hibernate(); }
   if (adcValue > 2000 && scrn < 1) {      // it's light, wake up
    scrn = 1;
    Serial.println("Screen wake up ");
    epdDisplay(); 
    screenUpdate();   }
  
  if (dif >= rt && temp2 > WRITE_TEMP)  {  //it's hot and time to write data
   startm = nowm;  // update start time
   dataWrite();    // write data
   dif = 0; scrn = 0;  }

   if (adcValue > 2000 && scrn > 0)   {   // refresh screen
    screenUpdate();}
   dataWrite(); 
   delay(1000);  
  }  // end loop
