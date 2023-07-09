#include <WiFi.h>
#include <TFT_eSPI.h>
#include "EEPROM.h"
#include "CG_RadSens.h"
#include "clipart.h"

const char* firmwareVersion="3.5";

String ssid;
String password;
String macAddress;
String macAddressShort;
String dataToNarodmon;
String dataFromNarodmon;

const char* host = "narodmon.ru";
const int httpPort = 8283;

long transferPeriod=300000;
long screenSaverPeriod=600000;
long transferPeriodToPrint=transferPeriod;
long pressTimerS1PB;
long pressTimerS2PB;
long nextTransfer;
long nextScreenUpdate;
long screenSaverTimer;
long nextBatteryIconUpdate;
long nextWiFiconnection;
long nextHostConnection;

bool lastStateS1PB;
bool lastStateS2PB;
bool shortPressS1PB;
bool shortPressS2PB;
bool longPressS1PB;
bool longPressS2PB;
bool changeParameter;
bool WiFiErase;
bool WiFiSet;
bool WiFiConfigured;
bool modeChanged=1;
bool settingChanged;
bool readingUnitSelector;
bool screenSaver;
bool screenSaverChanged;
bool buzzerOn=1;
bool radSMultiplicator;
volatile bool peep;

uint8_t counter;
uint8_t focusSetting;
uint8_t mode; //0-SEARCH MODE; 1-TRANSFER MODE; 2-SETTINGS; 3-INFO
uint8_t WiFiConnectionStatus; //0-NOT DEFINED; 1-FAULT; 2-POOR; 3-GOOD; 4-EXCELLENT
uint8_t clientConnectionStatus; //0-NOT DEFINED; 1-FAULT; 2-GOT RESPONCE; 3-OK 
uint8_t sensitivity=105;
uint16_t radReadingColor;
uint16_t doseReadingColor;

float radD;
float radS;
float dose;
float dosePrint;
float Voltage;
float VoltageCorrectionSpan=1.0000;

TFT_eSPI tft = TFT_eSPI(135, 240);
CG_RadSens radSens(RS_DEFAULT_I2C_ADDRESS);

void IRAM_ATTR buzzer() {peep=1;}

void setup() {
  pinMode(0, INPUT); // pushbutton II
  pinMode(17, INPUT_PULLUP); // pulse input from RadSens
  pinMode(27, OUTPUT); // buzzer
  pinMode(35, INPUT); // pushbutton I
  pinMode(12, OUTPUT); // cut RadSens "GND" in sleep mode
  pinMode(14, OUTPUT); // cut power supply in sleep mode

  attachInterrupt(17, buzzer, FALLING);

  digitalWrite(12, HIGH);
  digitalWrite(14, HIGH);

  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(0xFFFF);
  tft.setSwapBytes(true);
  tft.pushImage(102, 50,  37, 36, clipart);

  if (!EEPROM.begin(200)){
    tft.setTextColor(0xF800);
    tft.drawString("Failed to init EEPROM", 55, 110, 2);
  }
  else{
    ssid = readStringFromFlash(0);
    password = readStringFromFlash(40);
    macAddress = readStringFromFlash(60);
    if((EEPROM.read(80)<85)|(EEPROM.read(80)>125)) {
    EEPROM.write(80, sensitivity);
    EEPROM.commit();
    }
    sensitivity=EEPROM.read(80);
  }

  if (ssid.length()!=0) WiFiConfigured=1;

  Wire.begin();
  radSens.init();
  delay(100);

  while(!radSens.init()){
  tft.setTextColor(0xF800);
  tft.drawString("Sensor wiring error!", 55, 93, 2);
  }
  radSens.setSensitivity(sensitivity);
  radSens.setHVGeneratorState(1);
  radSens.setLedState(1);

  delay(2000);
}

void loop() {
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
  if(shortPressS2PB&screenSaver){
    shortPressS2PB=0;
    screenSaver=0;
    modeChanged=1;
    screenSaverChanged=1;
  }
  if(shortPressS2PB&!focusSetting){
    shortPressS2PB=0;
    mode++;
    if (mode==5) mode=0;
    modeChanged=1;
  }
  if(shortPressS2PB&(focusSetting>0)){
    shortPressS2PB=0;
    changeParameter=1;
    settingChanged=1;
  }
////////////////////////////////////////////////////////////////////
  if(longPressS2PB==1){
    longPressS2PB=0;
    digitalWrite(12, LOW);
    digitalWrite(14, LOW);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);
    esp_deep_sleep_start();
  }
////////////////////////////////////////////////////////////////////
  if((shortPressS1PB==1)&((mode==0)|(mode==1))){
    readingUnitSelector=!readingUnitSelector;
    shortPressS1PB=0;
  }
  if((shortPressS1PB==1)&(mode==2)&(WiFiConfigured)){
    shortPressS1PB=0;
    if(screenSaver){
      screenSaver=0;
      modeChanged=1;
      screenSaverChanged=1;
    }
    else screenSaver=1;
  }
  if((shortPressS1PB==1)&(mode==3)){
    shortPressS1PB=0;
    focusSetting++;
    settingChanged=1;
    if (focusSetting>=4) focusSetting=0;
  }
  if((shortPressS1PB==1)&(mode==4)) shortPressS1PB=0;
///////////////////////////////////////////////////////////////////
  if(longPressS1PB==1) longPressS1PB=0;
////////////////////////////////////////////////////////////////////
  if(!screenSaver&(millis()>nextBatteryIconUpdate)){
    Voltage = 3.6/4096*2*analogRead(34)*VoltageCorrectionSpan;
    if (Voltage<3.5){ //shutdown
      for(counter=1; counter<=10; counter++){
        tft.fillScreen(0x0000);
        delay(500);
        tft.drawSmoothRoundRect(80, 47, 4, 2, 80, 40, 0xF800, 0xF800, 0xF);
        tft.drawWideLine(164, 60, 164, 74, 3, 0xF800, 0xF800);
        delay(500);
      }
      digitalWrite(12, LOW);
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

  if (mode==0){
    if(modeChanged){
      modeChanged=0;
      nextScreenUpdate=0;
      nextBatteryIconUpdate=0;
      tft.fillScreen(0x0000);
    }
    if(millis()>=nextScreenUpdate){
      nextScreenUpdate=millis()+1000;
      radD=radSens.getRadIntensyDynamic();
      if (radD<20) radReadingColor=0x07E0;
      if (radD>=20) radReadingColor=0xFFE0;
      if (radD>=40) radReadingColor=0xFDA0;
      if (radD>=60) radReadingColor=0xF800;
      tft.setTextColor(radReadingColor, 0x0000, 0);
      if(readingUnitSelector) {
        if(radD>=99999.5) {
          radD=radD/100000;
          tft.drawString("mZv", 190, 51, 4);
        }
        else {
          radD=radD/100;
          tft.drawString(" uZv ", 190, 51, 4);
        }
      }
      else {
        if(radD>=999.5) {
          radD=radD/1000;
          tft.drawString(" mR ", 190, 51, 4);
        }
        else tft.drawString("  uR ", 190, 51, 4);
      }
      tft.drawWideLine(198, 77, 234, 77, 2, radReadingColor, radReadingColor);
      tft.drawString("h", 210, 84, 4);
      if (radD<9.99) tft.drawFloat(radD, 2, -2, 30, 8);
      if ((radD>=9.99)&(radD<99.95)) tft.drawFloat(radD, 1, -2, 30, 8);
      if (radD>=99.95){
        tft.drawNumber(radD, 22, 30, 8);
        tft.fillRect(0, 30, 22, 75, 0x0000);
      }
    }
  }

  if (mode==1) {
    if (modeChanged) {
      modeChanged=0;
      nextScreenUpdate=0;
      nextBatteryIconUpdate=0;
      dose=0;
      tft.fillScreen(0x0000);
      tft.setTextColor(0xC618);
      tft.drawString("Dose", 5, 8, 4);
      tft.setSwapBytes(true);
      tft.pushImage(5, 40,  37, 36, clipart);
      tft.pushImage(5, 90,  37, 36, clipart);
    }
    if(millis()>=nextScreenUpdate) {
      nextScreenUpdate=millis()+1000;
      radD=radSens.getRadIntensyDynamic();
      dose=dose+radD/3600;

      if (radD<20) radReadingColor=0x07E0;
      if (radD>=20) radReadingColor=0xFFE0;
      if (radD>=40) radReadingColor=0xFDA0;
      if (radD>=60) radReadingColor=0xF800;

      if (dose<10000000) doseReadingColor=0x07E0;   //0,10Zv
      if (dose>=10000000) doseReadingColor=0xFFE0;  //0,10Zv
      if (dose>=25000000) doseReadingColor=0xFDA0;  //0,25Zv
      if (dose>=100000000) doseReadingColor=0xF800; //1,00Zv
      
      if(readingUnitSelector) {
        tft.setTextColor(radReadingColor, 0x0000, 0);
        if(radD>=99999.5) {
          radD=radD/100000;
          tft.drawString("mZv/h", 166, 57, 4);
        }
        else {
          radD=radD/100;
          tft.drawString("uZv/h  ", 166, 57, 4);
        }
        tft.setTextColor(doseReadingColor, 0x0000, 0);
        if(dose<99999.5) {
          dosePrint=dose/100;
          tft.drawString("uZv", 176, 107, 4);
        }
        if((dose>=99999.5)&(dose<99999999.5)) {
          dosePrint=dose/100000;
          tft.drawString("mZv", 176, 107, 4);
        }
        if(dose>=99999999.5) {
          dosePrint=dose/100000000;
          tft.drawString(" Zv    ", 176, 107, 4);         
        }
      }
      else {
        tft.setTextColor(radReadingColor, 0x0000, 0);
        if(radD>=999.5) {
          radD=radD/1000;
          tft.drawString(" mR/h ", 166, 57, 4);
        }
        else {
          tft.drawString ("  uR/h ", 166, 57, 4);
        }
        tft.setTextColor(doseReadingColor, 0x0000, 0);
        if(dose<999.5) {
          dosePrint=dose;
          tft.drawString(" uR ", 176, 107, 4);
        }
        if((dose>=999.5)&(dose<999999.5)) {
          dosePrint=dose/1000;
          tft.drawString("mR  ", 176, 107, 4);
        }
        if(dose>=999999.5) {
          dosePrint=dose/1000000;
          tft.drawString("  R     ", 176, 107, 4);
        }
      }
      tft.setTextColor(radReadingColor, 0x0000, 0);
      if (radD<9.99) tft.drawFloat(radD, 2, 60, 40, 6);
      if ((radD>=9.99)&(radD<99.95)) tft.drawFloat(radD, 1, 60, 40, 6);
      if (radD>=99.95) {
        tft.drawNumber(radD, 73, 40, 6);
        tft.fillRect(60, 40, 13, 37, 0x0000);
      }
      Serial.println(dosePrint);
      tft.setTextColor(doseReadingColor, 0x0000, 0);
      if (dosePrint<9.99) tft.drawFloat(dosePrint, 2, 60, 90, 6);
      if ((dosePrint>=9.99)&(dosePrint<99.95)) tft.drawFloat(dosePrint, 1, 60, 90, 6);
      if (dosePrint>=99.95){
      tft.drawNumber(dosePrint, 73, 90, 6);
      tft.fillRect(60, 90, 13, 37, 0x0000);
      }
    }
  }

  if (mode==2){
    if(modeChanged){
      modeChanged=0;
      nextScreenUpdate=0;
      nextBatteryIconUpdate=0;
      tft.fillScreen(0x0000);
      tft.setTextColor(0xC618);
      tft.drawString("Narodmon", 5, 8, 4);
      if (WiFiConfigured){
        screenSaverTimer=millis()+screenSaverPeriod;
        tft.setSwapBytes(true);
        tft.pushImage(5, 50,  37, 36, clipart);
        tft.drawSmoothRoundRect(5, 108, 1, 0, 229, 16, 0xC618, 0xC618, 0xF);
        if(!screenSaverChanged){
          nextTransfer=millis()+transferPeriod;
          transferPeriodToPrint=transferPeriod;
        }
        else{
          delay(50);
          digitalWrite(TFT_BL, 1);
          screenSaverChanged=0;
        }
      }
      else{
        tft.setTextColor(0xF800);
        tft.drawString("No Wi-Fi Credentials", 5, 60, 4);
      }
    }

    if(WiFiConfigured){
      if(millis()>=nextScreenUpdate){
        nextScreenUpdate=millis()+1000;
        if(millis()>=screenSaverTimer)screenSaver=1;
        radS=radSens.getRadIntensyStatic();
        if(!screenSaver){
          if (radS<20) radReadingColor=0x07E0;
          if (radS>=20) radReadingColor=0xFFE0;
          if (radS>=40) radReadingColor=0xFDA0;
          if (radS>=60) radReadingColor=0xF800;
          tft.setTextColor(radReadingColor, 0x0000, 0);
         
          if(radS>=999.5) {
            radS=radS/1000;
            radSMultiplicator=1;
            tft.drawString("mR/h", 174, 66, 4);
          }
          else {
            radSMultiplicator=0;
            tft.drawString("uR/h", 182, 66, 4);
            tft.fillRect(174, 66, 8, 20, 0x0000);
          }
          if (radS<9.99) tft.drawFloat(radS, 2, 66, 49, 6);
          if ((radS>=9.99)&(radS<99.95)) tft.drawFloat(radS, 1, 66, 49, 6);
          if (radS>=99.95) {
            tft.drawNumber(radS, 80, 49, 6);
            tft.fillRect(66, 49, 14, 38, 0x0000);
          }
          tft.fillRect(8, 111, (224-(224*(nextTransfer-millis())/transferPeriodToPrint)), 11, 0x001F);
          switch(WiFiConnectionStatus){
            case 1:
            tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0xF800, 0xF800, true);
            tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0xF800, 0xF800, true);
            tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0xF800, 0xF800, true);
            break;
            case 2:
            tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0x03E0, 0x03E0, true);
            tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0x7BEF, 0x7BEF, true);
            tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0x7BEF, 0x7BEF, true);
            break;
            case 3:
            tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0x03E0, 0x03E0, true);
            tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0x03E0, 0x03E0, true);
            tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0x7BEF, 0x7BEF, true);
            break;
            case 4:
            tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0x03E0, 0x03E0, true);
            tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0x03E0, 0x03E0, true);
            tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0x03E0, 0x03E0, true);
            break;
            default:
            tft.drawSmoothArc(151, 26, 1, 2, 90, 270, 0x7BEF, 0x7BEF, true);
            tft.drawSmoothArc(151, 26, 8, 9, 130, 230, 0x7BEF, 0x7BEF, true);
            tft.drawSmoothArc(151, 26, 15, 16, 135, 225, 0x7BEF, 0x7BEF, true);
          }
          switch(clientConnectionStatus){
            case 1: tft.drawSmoothCircle(180, 18, 8, 0xF800, 0xF800); break;
            case 2: tft.drawSmoothCircle(180, 18, 8, 0xFFE0, 0xFFE0); break;
            case 3: tft.drawSmoothCircle(180, 18, 8, 0x03E0, 0x03E0); break;
            default: tft.drawSmoothCircle(180, 18, 8, 0x7BEF, 0x7BEF);
          }
        }
        else{
          digitalWrite(TFT_BL, 0);
          tft.fillScreen(0xFFFF);
        }
      }
      if(millis()>=nextTransfer){
        if(!screenSaver) tft.fillRect(8, 111, 224, 11, 0x0000);
        Serial.print("Connecting to ");
        Serial.println(ssid);
        WiFi.begin(ssid, password);
        counter=0;
        while (WiFi.status() != WL_CONNECTED){
          if(counter<5){
            counter++;    
            delay(500);
            Serial.print(".");
            delay(500);
          }      
          else{ 
            WiFi.disconnect(1,1);
            WiFiConnectionStatus=1;
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
        if (WiFi.RSSI()<-75) WiFiConnectionStatus=2;
        if ((WiFi.RSSI()>=-75)&(WiFi.RSSI()<-61)) WiFiConnectionStatus=3;
        if (WiFi.RSSI()>=-61) WiFiConnectionStatus=4;

        Serial.print("Connecting to ");
        Serial.println(host);
  
        WiFiClient client;
        delay(100);
        if (!client.connect(host, httpPort)){
          if(!screenSaver){
            clientConnectionStatus=1;
            tft.drawSmoothCircle(180, 18, 8, 0xF800, 0xF800);
          }
        Serial.print("Could not connect to ");    
        Serial.println(host);
        Serial.println("Next attempt in 60 sec");
        client.stop();
        WiFi.disconnect(1,1);  
        nextTransfer=millis()+60000;
        transferPeriodToPrint=60000;
        return;
        }
  
        macAddressShort=macAddress;
        macAddressShort.replace(":", "");
        if(radSMultiplicator) radS=radS*1000;
        dataToNarodmon="#"+macAddressShort+"\n#RAD#"+String(radS)+"\n##";

        Serial.println("Sending:");
        Serial.println(dataToNarodmon);
        client.print(dataToNarodmon);
        delay(100);
 
        Serial.println("Requesting: ");  
        while(client.connected()) {
          dataFromNarodmon = client.readStringUntil('\r');
          dataFromNarodmon.replace("\n", "");
          Serial.println(dataFromNarodmon);
        }
        if (dataFromNarodmon=="OK") clientConnectionStatus=3;
        else clientConnectionStatus=2;

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
  }

  if (mode==3){
    if (modeChanged==1){
      clientConnectionStatus=0;
      WiFiConnectionStatus=0;
      modeChanged=0;
      nextBatteryIconUpdate=0;
      focusSetting=0;
      tft.fillScreen(0x0000);
      tft.setTextColor(0xC618);
      tft.drawString("Settings", 5, 8, 4);
      tft.drawString("Buzzer:", 5, 40, 4);
      tft.drawString("Sensitivity:", 5, 72, 4);
      tft.drawString("Wi-Fi:", 5, 104, 4);
      if (buzzerOn) tft.drawString("on", 100, 40, 4);
      else tft.drawString("off", 100, 40, 4);
      tft.drawNumber(sensitivity, 140, 72, 4);
    }
    if (settingChanged==1){
      settingChanged=0;
      if (focusSetting==1){
        tft.fillRect(98, 40, 34, 24, 0x001F);
        if (changeParameter==1){
          buzzerOn=!buzzerOn;
          changeParameter=0;
        }
      }
      else tft.fillRect(98, 40, 34, 24, 0x0000);
      if (buzzerOn) tft.drawString("on", 100, 40, 4);
      else tft.drawString("off", 100, 40, 4);
    
      if (focusSetting==2){
        tft.fillRect(138, 71, 46, 24, 0x001F);
        if (changeParameter){
          changeParameter=0;
          sensitivity++;
          if (sensitivity>125) sensitivity=85;
          radSens.setSensitivity(sensitivity);
          EEPROM.write(80, sensitivity);
          EEPROM.commit();
        }
      }
      else tft.fillRect(138, 71, 46, 24, 0x0000);
      tft.drawNumber(sensitivity, 140, 72, 4);

      if (focusSetting==3){
        tft.fillRect(78, 104, 66, 24, 0x001F); // 00->10->01->00
          if (changeParameter==1){
            if(!WiFiErase) WiFiSet=!WiFiSet;
            if(!WiFiSet) WiFiErase=!WiFiErase;
            changeParameter=0;
          }
        }
        else tft.fillRect(78, 104, 66, 24, 0x0000);
        if (WiFiSet) tft.drawString("set", 96, 104, 4);
        if (WiFiErase) tft.drawString("erase", 80, 104, 4);
        if (!WiFiErase&!WiFiSet) tft.drawString("", 84, 104, 4);

        if ((focusSetting==0)&WiFiErase){
          WiFiErase=0;
          modeChanged=1;
          WiFiConfigured=0;
          ssid="";
          password="";
          macAddress="";
          writeStringToFlash("", 0);
          writeStringToFlash("", 40);
          writeStringToFlash("", 60);
          tft.fillScreen(0x0000);
          tft.setTextColor(0xF800);
          tft.drawString("Wi-Fi Credentials", 25, 44, 4);
          tft.drawString("Erased", 80, 74, 4);
          delay(3000);
        }

      if ((focusSetting==0)&WiFiSet){
        WiFiSet=0;
        modeChanged=1;
        WiFi.mode(WIFI_AP_STA);
        WiFi.beginSmartConfig();
        tft.fillScreen(0x0000);
        tft.setTextColor(0xC618);
        tft.drawString("SmartConfig", 5, 8, 4);
        tft.drawString("1. Start EspTouch", 5, 40, 4);
        tft.drawString("2. Input Password", 5, 72, 4);
        tft.drawString("3. Press Confirm", 5, 104, 4);

        counter=60;
        while (!WiFi.smartConfigDone()) {
          if(counter>0){
            tft.fillRect(175, 8, 28, 24, 0x0000);
            tft.drawNumber(counter, 175, 8, 4);
            counter--;
            delay(1000);
          }      
          else{ 
            WiFi.stopSmartConfig();
            WiFi.disconnect(1,1);
            tft.fillScreen(0x0000);
            tft.setTextColor(0xF800);
            tft.drawString("SmartConfig", 50, 44, 4);
            tft.drawString("Time Over", 60, 74, 4);
            delay (5000);
            return;
          }  
        }
        tft.fillScreen(0x0000);
        tft.setTextColor(0x07E0);
        tft.drawString("Wi-Fi Credentials", 25, 44, 4);
        tft.drawString("Recieved", 75, 74, 4);

        ssid = WiFi.SSID();
        password = WiFi.psk();
        macAddress=WiFi.macAddress();
        WiFiConfigured=1;

        WiFi.stopSmartConfig();
        WiFi.disconnect(1,1);
        writeStringToFlash(ssid.c_str(), 0); // storing ssid at address 0
        writeStringToFlash(password.c_str(), 40); // storing pss at address 40
        writeStringToFlash(macAddress.c_str(), 60); // storing pss at address 60
        delay(3000);
      }
    }
  }

  if (mode==4){
    if (modeChanged==1){
      modeChanged=0;
      nextBatteryIconUpdate=0;
      tft.fillScreen(0x0000);
      tft.setTextColor(0xC618);
      tft.drawString("Info", 5, 8, 4);
      tft.drawString("Firmware Version:", 5, 40, 2);
      tft.drawString("RadSens Firmware Version:", 5, 58, 2);
      tft.drawString("RadSens Chip ID:", 5, 76, 2);
      tft.drawString("SSID:", 5, 94, 2);
      tft.drawString("MAC:", 5, 112, 2);

      tft.drawString(firmwareVersion, 122, 40, 2);
      tft.drawNumber(radSens.getFirmwareVersion(), 178, 58, 2);
      tft.drawNumber(radSens.getChipId(), 118, 76, 2);
      tft.drawString(ssid, 44, 94, 2);
      tft.drawString(macAddress, 44, 112, 2);
    }
  }

  if(peep) {
    peep=0;
    if(buzzerOn&((mode==0)|(mode==1))){
      digitalWrite(27, HIGH);
      delay(20);
      digitalWrite(27, LOW);
    }
  }
}

void writeStringToFlash(const char* toStore, int startAddr) {
  for (counter=0; counter < (strlen(toStore) + 1); counter++) {
    EEPROM.write(startAddr + counter, toStore[counter]);
  }
  EEPROM.write(startAddr + counter, '\0');
  EEPROM.commit();
}

String readStringFromFlash(int startAddr){
  char in[128];
  for (counter=0; counter<128; counter++){
    in[counter] = EEPROM.read(startAddr + counter);
  }
  return String(in);
}
