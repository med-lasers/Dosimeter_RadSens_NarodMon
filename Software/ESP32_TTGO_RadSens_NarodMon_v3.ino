#include <WiFi.h>
#include "CG_RadSens.h"
#include <TFT_eSPI.h>
#include "clipart.h"

TFT_eSPI tft = TFT_eSPI(135, 240);
#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP  300

const char* ssid     = "input_your_ssid_here";
const char* password = "input_your_password_here";
 
const char* host = "narodmon.ru";
const int httpPort = 8283;

String dataToNarodmon;
String dataFromNarodmon;

long transferPeriod=300000;
long transferPeriodToPrint=transferPeriod;

bool lastStateS1PB;
bool lastStateS2PB;
bool shortPressS1PB=0;
bool shortPressS2PB=0;
bool longPressS1PB=0;
bool longPressS2PB=0;
bool changeParameter;
uint8_t focusSetting=0;
long pressTimerS1PB;
long pressTimerS2PB;

uint32_t lastNumberOfPulses;
uint8_t mode=0; //0-SEARCH MODE; 1-TRANSFER MODE; 2-SETTINGS
uint16_t sensitivity=105;
bool modeChanged=1;
bool settingChanged;

bool buzzer=1;

bool readingUnitSelector;
uint16_t readingColor;

long nextTransfer;
long nextScreenUpdate=0;
long nextBatteryIconUpdate=0;
long nextWiFiconnection=0;
long nextHostConnection=0;
float radD;
float radS;
float dose;
float dosePrint;
float Voltage;
int counter=0;
int y;
int i;

CG_RadSens radSens(RS_DEFAULT_I2C_ADDRESS);

void setup() {
  pinMode(0, INPUT);
  pinMode(35, INPUT);
  pinMode(14, OUTPUT);
  pinMode(27, OUTPUT);

  digitalWrite(14, HIGH);
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(0xFFFF);
  tft.setSwapBytes(true);
  tft.pushImage(102, 50,  37, 36, clipart);

  Wire.begin(); // This function initializes the Wire library
  delay(100);

  while(!radSens.init()){
    tft.setTextColor(0xF800);
    tft.drawString("Sensor wiring error!", 55, 93, 2);
    delay(1000);
    }
  radSens.setSensitivity(105);
  radSens.setHVGeneratorState(true);
  radSens.setLedState(false);
  tft.setTextColor(0x7BEF);
  tft.drawNumber(radSens.getFirmwareVersion(), 113, 93, 2);
  delay(2000);
}
 
void loop(){
//  Serial.println(radSens.getSensitivity());
  ////////////////////____ II ______///////////////////////////////
  if (!digitalRead(0)&(lastStateS1PB==0)){
    pressTimerS1PB=millis();
    lastStateS1PB=1;
  }
  if (digitalRead(0)&(lastStateS1PB==1)){
    if ((millis()-pressTimerS1PB)>2000) longPressS1PB=1;
    if ((millis()-pressTimerS1PB)>100) shortPressS1PB=1;
    lastStateS1PB=0;
  }

///////////////////////////////////////////////////////////////////

/////////////////////____ I _____ /////////////////////////////////
  if (!digitalRead(35)&(lastStateS2PB==0)){
    pressTimerS2PB=millis();
    lastStateS2PB=1;
  }
  if (digitalRead(35)&(lastStateS2PB==1)){
    if ((millis()-pressTimerS2PB)>2000) longPressS2PB=1;
    if ((millis()-pressTimerS2PB)>100) shortPressS2PB=1;
    lastStateS2PB=0;
  }
////////////////////////////////////////////////////////////////////
  if((shortPressS2PB==1)&(focusSetting==0)){
    shortPressS2PB=0;
    mode++;
    if (mode==4) mode=0;
    modeChanged=1;
  }
  if((shortPressS2PB==1)&(focusSetting>0)){
    shortPressS2PB=0;
    changeParameter=1;
    settingChanged=1;
  };
////////////////////////////////////////////////////////////////////
  if(longPressS2PB==1){
    longPressS2PB=0;
    digitalWrite(14, LOW);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);
    esp_deep_sleep_start();
  }
////////////////////////////////////////////////////////////////////
  if((shortPressS1PB==1)&((mode==0)|(mode==1))){
    readingUnitSelector=!readingUnitSelector;
    shortPressS1PB=0;
  }
  if((shortPressS1PB==1)&(mode==2)) shortPressS1PB=0;
  if((shortPressS1PB==1)&(mode==3)){
    shortPressS1PB=0;
    focusSetting++;
    settingChanged=1;
    if (focusSetting>=3) focusSetting=0;
  }
    
///////////////////////////////////////////////////////////////////
  if(longPressS1PB==1) longPressS1PB=0;
////////////////////////////////////////////////////////////////////

  if(millis()>nextBatteryIconUpdate){
    Voltage = 3.6/4096*2*analogRead(34)*0.9963;
    if (Voltage<3.5){ //shutdown
      for(i=1; i<=10; i++){
        tft.fillScreen(0x0000);
        delay(500);
        tft.drawSmoothRoundRect(80, 47, 4, 2, 80, 40, 0xF800, 0xF800, 0xF);
        tft.drawWideLine(164, 60, 164, 74, 3, 0xF800, 0xF800);
        delay(500);
      }
      digitalWrite(14, LOW);
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);
      esp_deep_sleep_start();
    }
    if ((Voltage>=3.5)&(Voltage<3.63)){ //0-20%
      tft.drawSmoothRoundRect(200, 11, 1, 0, 28, 15, 0xF800, 0xF800, 0xF);
      tft.drawWideLine(231, 16, 231, 21, 2, 0xF800, 0xF800);
      tft.fillRect(203, 14, 23, 10, 0x0000);
    }
    else{
      tft.drawSmoothRoundRect(200, 11, 1, 0, 28, 15, 0xC618, 0xC618, 0xF);
      tft.drawWideLine(231, 16, 231, 21, 2, 0xC618, 0xC618);
      tft.fillRect(203, 14, 23, 10, 0x0000);
    }
    if ((Voltage>=3.63)&(Voltage<3.7)){ //20-40%
      tft.fillRect(203, 14, 5, 10, 0xFDA0);
    }
    if ((Voltage>=3.7)&(Voltage<3.78)){ //40-60%
      tft.fillRect(203, 14, 5, 10, 0xC618);
      tft.fillRect(209, 14, 5, 10, 0xC618);
    }
    if ((Voltage>=3.78)&(Voltage<3.92)){ //60-80%
      tft.fillRect(203, 14, 5, 10, 0xC618);
      tft.fillRect(209, 14, 5, 10, 0xC618);
      tft.fillRect(215, 14, 5, 10, 0xC618);
    }
    if ((Voltage>=3.92)&(Voltage<4.2)){ //80-100%
      tft.fillRect(203, 14, 5, 10, 0xC618);
      tft.fillRect(209, 14, 5, 10, 0xC618);
      tft.fillRect(215, 14, 5, 10, 0xC618);
      tft.fillRect(221, 14, 5, 10, 0xC618);
    }
    if (Voltage>=4.2){ //charging
      tft.fillRect(203, 14, 23, 10, 0x001F);
    }
    nextBatteryIconUpdate=millis()+5000;
  }
////////drawing speaker////////
// tft.fillRect(173, 7, 6, 7, 0xFFFF);
// tft.fillTriangle(175, 10, 182, 3, 182, 17, 0xFFFF);
// tft.fillRect(182, 3, 2, 15, 0xFFFF);
// tft.drawSmoothArc(176, 10, 12, 12, 255, 285, 0xFFFF, 0xFFFF, true);
// tft.drawSmoothArc(176, 10, 18, 18, 245, 295, 0xFFFF, 0xFFFF, true);

     radD=radSens.getRadIntensyDynamic();
     radS=radSens.getRadIntensyStatic();
    // radD=Voltage*3;
    // radS=Voltage*5;

  if (mode==0){
    if (modeChanged==1){
      modeChanged=0;
      nextScreenUpdate=0;
      nextBatteryIconUpdate=0;
      lastNumberOfPulses=radSens.getNumberOfPulses();
      tft.fillScreen(0x0000);
    }
 //   Serial.println(radSens.getNumberOfPulses());
    if (buzzer){
    if (radSens.getNumberOfPulses()>lastNumberOfPulses){
    digitalWrite(27, HIGH);
    delay(50);
    digitalWrite(27, LOW);
    delay(50);
    lastNumberOfPulses++;
    }
    }
    if(millis()>=nextScreenUpdate){
      if (radD<20) readingColor=0x07E0;
      if (radD>=20) readingColor=0xFFE0;
      if (radD>=40) readingColor=0xFDA0;
      if (radD>=60) readingColor=0xF800;
      tft.setTextColor(readingColor);
    
      tft.fillRect(1, 30, 236, 74, 0x0000);    
    
      if(readingUnitSelector==1){
        tft.drawString("uZv", 196, 51, 4);
        radD=radD/100;
      }
      else tft.drawString("uR", 201, 51, 4);

      if (radD<9.99) tft.drawFloat(radD, 2, 1, 30, 8);
      if ((radD>=9.99)&(radD<99.95)) tft.drawFloat(radD, 1, 1, 30, 8);
      if (radD>=99.95) tft.drawNumber(radD, 1, 30, 8);

      tft.drawWideLine(199, 77, 235, 77, 2, readingColor, readingColor);
      tft.drawString("h", 212, 84, 4);

      nextScreenUpdate=millis()+1000;
    }
  }

  if (mode==1){
    if (modeChanged==1){
      modeChanged=0;
      nextScreenUpdate=0;
      nextBatteryIconUpdate=0;
      lastNumberOfPulses=radSens.getNumberOfPulses();
      tft.fillScreen(0x0000);
      tft.setTextColor(0xC618);
      tft.drawString("Dose", 5, 8, 4);
      tft.setSwapBytes(true);
      tft.pushImage(5, 40,  37, 36, clipart);
      tft.pushImage(5, 90,  37, 36, clipart);
    }
    if (buzzer){
    if (radSens.getNumberOfPulses()>lastNumberOfPulses){
      digitalWrite(27, HIGH);
      delay(50);
      digitalWrite(27, LOW);
      delay(50);
      lastNumberOfPulses++;
    }
    }
    if(millis()>=nextScreenUpdate){
      dose=dose+radD/3600;
      if (radD<20) readingColor=0x07E0;
      if (radD>=20) readingColor=0xFFE0;
      if (radD>=40) readingColor=0xFDA0;
      if (radD>=60) readingColor=0xF800;
      tft.setTextColor(readingColor);
    
      tft.fillRect(50, 40, 180, 90, 0x0000);

      if(readingUnitSelector==1){
        tft.drawString("uZv/h", 166, 57, 4);
        tft.drawString("uZv", 176, 107, 4);
        radD=radD/100;
        dosePrint=dose/100;
      }
      else{
        tft.drawString("uR/h", 170, 57, 4);
        tft.drawString("uR", 178, 107, 4);
        dosePrint=dose;
      }

      if (radD<9.99) tft.drawFloat(radD, 2, 60, 40, 6);
      if ((radD>=9.99)&(radD<99.95)) tft.drawFloat(radD, 1, 60, 40, 6);
      if (radD>=99.95) tft.drawNumber(radD, 60, 40, 6);

      if (dose<9.99) tft.drawFloat(dosePrint, 2, 60, 90, 6);
      if ((dose>=9.99)&(dose<99.95)) tft.drawFloat(dosePrint, 1, 60, 90, 6);
      if (dose>=99.95) tft.drawNumber(dosePrint, 60, 90, 6);

      nextScreenUpdate=millis()+1000;
    }
  }

  if (mode==2){
    if (modeChanged==1){
      modeChanged=0;
      nextScreenUpdate=0;
      nextBatteryIconUpdate=0;
      nextTransfer=millis()+transferPeriod;
      transferPeriodToPrint=transferPeriod;
      tft.fillScreen(0x0000);
      tft.setTextColor(0xC618);
      tft.drawString("Narodmon", 5, 8, 4);
      tft.setSwapBytes(true);
      tft.pushImage(5, 50,  37, 36, clipart);

      tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0x7BEF, 0x7BEF, true);
      tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0x7BEF, 0x7BEF, true);
      tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0x7BEF, 0x7BEF, true);

      tft.drawSmoothCircle(180, 18, 8, 0x7BEF, 0x7BEF);

      tft.drawSmoothRoundRect(5, 108, 1, 0, 229, 16, 0xC618, 0xC618, 0xF);
    }
    if(millis()>=nextScreenUpdate){
      if (radS<20) readingColor=0x07E0;
      if (radS>=20) readingColor=0xFFE0;
      if (radS>=40) readingColor=0xFDA0;
      if (radS>=60) readingColor=0xF800;
      tft.setTextColor(readingColor);
    
      tft.fillRect(67, 50, 92, 36, 0x0000);
      if (radS<9.99) tft.drawFloat(radS, 2, 66, 49, 6);
      if ((radS>=9.99)&(radS<99.95)) tft.drawFloat(radS, 1, 66, 49, 6);
      if (radS>=99.95) tft.drawNumber(radS, 66, 49, 6);
      tft.drawString("uR/h", 182, 66, 4);

      tft.fillRect(8, 111, (224-(224*(nextTransfer-millis())/transferPeriodToPrint)), 11, 0x001F);

      nextScreenUpdate=millis()+1000;
  }

  if(millis()>=nextTransfer){
    tft.fillRect(8, 111, 224, 11, 0x0000);
 //   if((millis()>=nextWiFiconnection)&(millis()>=nextHostConnection)){
      Serial.print("Connecting to ");
      Serial.println(ssid);
      WiFi.begin(ssid, password);
      counter=0;

      while (WiFi.status() != WL_CONNECTED) {
        if(counter<5){
          counter++;
         // tft.fillRect(140, 0, 30, 30, 0x0000); 
          //tft.fillRect(219, 7, 11, 19, 0xFFE0);     
          delay(500);
         // tft.fillRect(219, 7, 11, 19, 0x0000);
         //tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0xFFFF, 0xFFFF, true);
        // tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0xFFFF, 0xFFFF, true);
        // tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0xFFFF, 0xFFFF, true);
          Serial.print(".");
          delay(500);
        }      
        else{ 
          WiFi.disconnect(1,1);
         tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0xF800, 0xF800, true);
         tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0xF800, 0xF800, true);
         tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0xF800, 0xF800, true);
         tft.drawSmoothCircle(180, 18, 8, 0xF800, 0xF800);
          Serial.println();
          Serial.print("Could not connect to ");
          Serial.println(ssid);
          Serial.println("Next attempt in 60 sec");
          nextTransfer=millis()+60000;
          transferPeriodToPrint=60000;
          return;
        }
      }

    Serial.println();
    Serial.println("WiFi connected");  
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    Serial.println(WiFi.RSSI());
    if (WiFi.RSSI()<-75){
      tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0xC618, 0xC618, true);
      tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0x7BEF, 0x7BEF, true);
      tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0x7BEF, 0x7BEF, true);
    }
    if ((WiFi.RSSI()>=-75)&(WiFi.RSSI()<-61)){
      tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0xC618, 0xC618, true);
      tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0xC618, 0xC618, true);
      tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0x7BEF, 0x7BEF, true);
    }
    if (WiFi.RSSI()>=-61){
      tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0xC618, 0xC618, true);
      tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0xC618, 0xC618, true);
      tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0xC618, 0xC618, true);
    }
    ////////wi-fi////////
    
  
    Serial.print("Connecting to ");
    Serial.println(host);
  
    WiFiClient client;
    delay(100);
    if (!client.connect(host, httpPort)) {
      tft.drawSmoothCircle(180, 18, 8, 0xF800, 0xF800);
      Serial.print("Could not connect to ");    
      Serial.println(host);
      Serial.println("Next attempt in 60 sec");
      client.stop();
      WiFi.disconnect(1,1);  
      nextTransfer=millis()+60000;
      transferPeriodToPrint=60000;
      return;
    }
  
    dataToNarodmon="#PUT_YOUR_MAC_HERE\n#RAD#"+String(radS)+"\n##";

    Serial.println("Sending:");
    Serial.println(dataToNarodmon);
    client.print(dataToNarodmon);
    delay(100);
 
    Serial.println("Requesting: ");  
    while(client.available()) {
      dataFromNarodmon = client.readStringUntil('\r');
      Serial.println(dataFromNarodmon);
    }
  tft.drawSmoothCircle(180, 18, 8, 0xC618, 0xC618);

    Serial.println("Closing connection");
    client.stop();
 
    WiFi.disconnect(1,1);
    Serial.println("Disconnecting WiFi");

    Serial.print("Heap size: ");
    Serial.println(ESP.getFreeHeap());
    nextTransfer=millis()+transferPeriod;
    transferPeriodToPrint=transferPeriod;
  }
  }

  if (mode==3){
    if (modeChanged==1){
      modeChanged=0;
      nextBatteryIconUpdate=0;
      focusSetting=0;
      tft.fillScreen(0x0000);
      tft.setTextColor(0xC618);
      tft.drawString("Settings", 5, 8, 4);
      tft.drawString("Buzzer:", 5, 40, 4);
      tft.drawString("Sensitivity:", 5, 72, 4);
      tft.drawString("Set Wi-Fi:", 5, 104, 4);
      if (buzzer) tft.drawString("on", 100, 40, 4);
      else tft.drawString("off", 100, 40, 4);
      tft.drawNumber(sensitivity, 140, 72, 4);

    }
    if (settingChanged==1){
      settingChanged=0;

    if (focusSetting==1){
      tft.fillRect(100, 40, 30, 20, 0x001F);
      if (changeParameter==1){
        buzzer=!buzzer;
        changeParameter=0;
      }
    }
    else tft.fillRect(100, 40, 30, 20, 0x0000);
    if (buzzer) tft.drawString("on", 100, 40, 4);
    else tft.drawString("off", 100, 40, 4);
    
      if (focusSetting==2){
      tft.fillRect(140, 72, 41, 20, 0x001F);
      if (changeParameter){
        sensitivity++;
        if (sensitivity>=121) sensitivity=80;
        changeParameter=0;
      }
    }
    else tft.fillRect(140, 72, 41, 20, 0x0000);
    tft.drawNumber(sensitivity, 140, 72, 4);
    radSens.setSensitivity(sensitivity);
}
}
}
