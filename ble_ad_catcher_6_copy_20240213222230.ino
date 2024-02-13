/* The MIT License (MIT)
* 
* Copyright (c) 2023 Sergey Dronsky (Winnie_The_Pooh)
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.

Based on the work of pvvx: https://github.com/pvvx/ATC_MiThermometer/tree/master/esp32
*/

// Use partition scheme with more than 2 mb app

#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <cppQueue.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <sstream>

#include "esp32/rom/rtc.h"

String rsr1;
String vrsr1;
String rsr2;
String vrsr2;

#define PowerRelay 32

#include "c_Page_favicon.h"

#include <esp_task_wdt.h>
//120 seconds WDT
#define WDT_TIMEOUT 120

#include "time.h"
#include "sntp.h"

const char* ntpServer3 = "ntp21.vniiftri.ru";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

const long  gmtOffset_sec = 10800;
const int   daylightOffset_sec = 0;
bool rt_rdy = false;

/*A watched task can be unsubscribed from the TWDT using esp_task_wdt_delete(). 
 * A task that has been unsubscribed should no longer call esp_task_wdt_reset(). 
 * Once all tasks have unsubscribed form the TWDT, the TWDT can be 
 * deinitialized by calling esp_task_wdt_deinit().
Source – https://espressif-docs.readthedocs-hosted.com/projects/esp-idf/en/stable/api-reference/system/wdts.html

REPLY
ALEKSEI
August 27, 2022 at 9:37 pm
The TWDT can be initialized by calling esp_task_wdt_init() which will 
configure the hardware timer. A task can then subscribe to the TWDT 
using esp_task_wdt_add() in order to be watched. Each subscribed task must 
periodically call esp_task_wdt_reset() to reset the TWDT. 
Failure by any subscribed tasks to periodically call esp_task_wdt_reset() indicates 
that one or more tasks have been starved of CPU time or are stuck in a loop somewhere.

A watched task can be unsubscribed from the TWDT using esp_task_wdt_delete(). 
A task that has been unsubscribed should no longer call esp_task_wdt_reset(). 
Once all tasks have unsubscribed form the TWDT, the TWDT can be deinitialized 
by calling esp_task_wdt_deinit().
*/

int scanTime = 15; // seconds 30
BLEScan* pBLEScan;

const uint8_t max_ct = 10; // max number of ble points to listen
uint8_t amac[max_ct][6]; //array of MAC

//String knownBLEAddresses[] = {"6E:bc:55:18:cf:7b", "53:3c:cb:56:36:02", "40:99:4b:75:7d:2f", "5c:5b:68:6f:34:96"};
//if (strcmp(advertisedDevice.getAddress().toString().c_str(), knownBLEAddresses[i].c_str()) == 0)
//https://microkontroller.ru/esp32-projects/obnaruzhenie-ble-ustrojstv-s-pomoshhyu-modulya-esp32/?ysclid=lgt9wgrvbs928227969 

uint8_t bmac[max_ct][6]; //array to hold known device MACs to find their names (position) after scan
String smac[max_ct]; // array to hold device names (positions) 
float atemp[max_ct];
float humi[max_ct];
float vbatt[max_ct];
uint8_t pbatt[max_ct];
uint8_t mac_ct[max_ct];
//uint8_t mass[6];
uint8_t mass[6] = {0,0,0,0,0,0};
//uint8_t bass[6];
String sBLEdata;

WiFiMulti wifiMulti;

// WiFi connect timeout per AP. Increase when connecting takes longer.
const uint32_t connectTimeoutMs = 10000;

WebServer server(80);

String content;
String st;
int statusCode;
int statusWiFi = 1;

String f = String(__FILE__); //compiler predefined macro, returns file name
String t = String(__TIME__); //returns time when compile
String c = f.substring(f.lastIndexOf('\\')+1, f.indexOf('.')) + " " + String(__DATE__) + " " + t.substring(0, t.lastIndexOf(':'));

uint32_t chipId = 0;

int ct_rec=0;
String http_name = "esp32_ble";

uint32_t time1minStart = 0;
uint32_t OneMin = 60000;

void printBuffer(uint8_t* buf, int len) {
  for (int i = 0; i < len; i++) {
    Serial.printf("%02x", buf[i]);
  }
  Serial.print("\n");
}

String sprintBuffer(uint8_t* buf, int len) {
  String sbuffer="";
  static char tt[3]; //String tt="";

  for (int i = 0; i < len; i++) {
    sprintf(tt,"%02x", buf[i]);
    sbuffer += tt;
  }
 return sbuffer;
}

void parse_value(uint8_t* buf, int len) {
  int16_t x = buf[3];
  if (buf[2] > 1)
    x |=  buf[4] << 8;
  switch (buf[0]) {
    case 0x0D:
      if (buf[2] && len > 6) {
        float temp = x / 10.0;
        x =  buf[5] | (buf[6] << 8);
        float humidity = x / 10.0;
        Serial.printf("Temp: %.1f°, Humidity: %.1f %%\n", temp, humidity);
      }
      break;
    case 0x04: {
        float temp = x / 10.0;
        Serial.printf("Temp: %.1f°\n", temp);
      }
      break;
    case 0x06: {
        float humidity = x / 10.0;
        Serial.printf("Humidity: %.1f%%\n", humidity);
      }
      break;
    case 0x0A: {
        Serial.printf("Battery: %d%%", x);
        if (len > 5 && buf[4] == 2) {
          uint16_t battery_mv = buf[5] | (buf[6] << 8);
          Serial.printf(", %d mV", battery_mv);
        }
        Serial.printf("\n");
      }
      break;
    default:
      Serial.printf("Type: 0x%02x ", buf[0]);
      printBuffer(buf, len);
      break;
  }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

    uint8_t* findServiceData(uint8_t* data, size_t length, uint8_t* foundBlockLength) {
      uint8_t* rightBorder = data + length;
      while (data < rightBorder) {
        uint8_t blockLength = *data + 1;
        //Serial.printf("blockLength: 0x%02x\n",blockLength);
        if (blockLength < 5) {
          data += blockLength;
          continue;
        }
        uint8_t blockType = *(data + 1);
        uint16_t serviceType = *(uint16_t*)(data + 2);
        //Serial.printf("blockType: 0x%02x, 0x%04x\n", blockType, serviceType);
        if (blockType == 0x16) { // https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/
          // Serial.printf("blockType: 0x%02x, 0x%04x\n", blockType, serviceType);
          /* 16-bit UUID for Members 0xFE95 Xiaomi Inc. https://btprodspecificationrefs.blob.core.windows.net/assigned-values/16-bit%20UUID%20Numbers%20Document.pdf */
          if (serviceType == 0xfe95 || serviceType == 0x181a) { // mi or custom service
            //Serial.printf("blockLength: 0x%02x\n",blockLength);
            //Serial.printf("blockType: 0x%02x, 0x%04x\n", blockType, serviceType);
            *foundBlockLength = blockLength;
            return data;
          }
        }
        data += blockLength;
      }
      return nullptr;
    }

    void onResult(BLEAdvertisedDevice advertisedDevice) {
      uint8_t mac[6];
      uint8_t* payload = advertisedDevice.getPayload();
      size_t payloadLength = advertisedDevice.getPayloadLength();
      uint8_t serviceDataLength = 0;
      uint8_t* serviceData = findServiceData(payload, payloadLength, &serviceDataLength);

//      server.handleClient();
      
      if (serviceData == nullptr || serviceDataLength < 15)
        return;
      uint16_t serviceType = *(uint16_t*)(serviceData + 2);
      Serial.printf("Found service '%04x' data len: %d, ", serviceType, serviceDataLength);
      printBuffer(serviceData, serviceDataLength);
      if (serviceType == 0xfe95) {
        if (serviceData[5] & 0x10) {
          mac[5] = serviceData[9];
          mac[4] = serviceData[10];
          mac[3] = serviceData[11];
          mac[2] = serviceData[12];
          mac[1] = serviceData[13];
          mac[0] = serviceData[14];
          Serial.printf("MAC: "); printBuffer(mac, 6);
        }
        if ((serviceData[5] & 0x08) == 0) { // not encrypted
          serviceDataLength -= 15;
          payload = &serviceData[15];
          while (serviceDataLength > 3) {
            parse_value(payload, serviceDataLength);
            serviceDataLength -= payload[2] + 3;
            payload += payload[2] + 3;
          }
          Serial.printf("count: %d\n", serviceData[8]);
        } else {
          if (serviceDataLength > 19) { // aes-ccm  bindkey
            // https://github.com/ahpohl/xiaomi_lywsd03mmc
            // https://github.com/Magalex2x14/LYWSD03MMC-info
            Serial.printf("Crypted data[%d]! ", serviceDataLength - 15);
          }
          Serial.printf("count: %d\n", serviceData[8]);
        }
      } else { // serviceType == 0x181a
        if(serviceDataLength > 18) { // custom format
          mac[5] = serviceData[4];
          mac[4] = serviceData[5];
          mac[3] = serviceData[6];
          mac[2] = serviceData[7];
          mac[1] = serviceData[8];
          mac[0] = serviceData[9];
        
          Serial.printf("MAC: ");
          printBuffer(mac, 6);
          float temp = *(int16_t*)(serviceData + 10) / 100.0;
          float humidity = *(uint16_t*)(serviceData + 12) / 100.0;
          uint16_t vbat = *(uint16_t*)(serviceData + 14);
          Serial.printf("Temp: %.2f°, Humidity: %.2f%%, Vbatt: %d, Battery: %d%%, flg: 0x%02x, cout: %d\n", temp, humidity, vbat, serviceData[16], serviceData[18], serviceData[17]);

          for(int i=0; i < max_ct; i++){
            
            if(memcmp(mass, amac[i], 6) == 0){ //true if amac[i]==0

              atemp[i] = temp;
              humi[i] = humidity;
              vbatt[i] = vbat; ///1000.0;
              pbatt[i] = serviceData[16];
 
              memmove(amac[i], mac, 6);
              break;
            }

      if(memcmp(mac, amac[i], 6) == 0) { // if arrays are EQ
              atemp[i] = temp;
              humi[i] = humidity;
              vbatt[i] = vbat/1000.0;
              pbatt[i] = serviceData[16];
              ++mac_ct[i];
              break;
            }
          }
        
        } else if(serviceDataLength == 17) { // format atc1441
          Serial.printf("MAC: "); printBuffer(serviceData + 4, 6);
          int16_t x = (serviceData[10]<<8) | serviceData[11];
          float temp = x / 10.0;
          uint16_t vbat = x = (serviceData[14]<<8) | serviceData[15];
          Serial.printf("Temp: %.1f°, Humidity: %d%%, Vbatt: %d, Battery: %d%%, cout: %d\n", temp, serviceData[12], vbat, serviceData[13], serviceData[16]);
        }
      }
    }
};

void setup() {
  Serial.begin(115200);
  
   xTaskCreate(
    toggleLED,    // Function that should be called
    "toggleLED",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

  rsr1 = "CPU0 reset reason: ";
  rsr1 += print_reset_reason(rtc_get_reset_reason(0));
  rsr1 += " " + verbose_print_reset_reason(rtc_get_reset_reason(0));

  rsr2 = "CPU1 reset reason: ";
  rsr2 += print_reset_reason(rtc_get_reset_reason(1));
  rsr2 += " " + verbose_print_reset_reason(rtc_get_reset_reason(1));

 printf("\nStart\n Version %s\n", c.c_str());

  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  Serial.printf("ESP32 Chip model = %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("This chip has %d cores\n", ESP.getChipCores());
  Serial.print("Chip ID: "); Serial.println(chipId);
  Serial.println(rsr1);
  Serial.println(rsr2);
   
  WiFi.mode(WIFI_STA); // только клиент

        // Register multi WiFi networks
        wifiMulti.addAP("AP1_name", "AP1_pass");
        wifiMulti.addAP("AP2_name", "AP2_pass");
        wifiMulti.addAP("AP3_name", "AP3_pass");

        Serial.println("Trying to connect");
        for (int i=0; i<3; i++) {
            if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
                      
              Serial.print("WiFi connected: ");
              Serial.print(WiFi.SSID());
              Serial.print(" ");
              Serial.println(WiFi.localIP());
              break;
                } else {
                  Serial.println(String(i) + " WiFi not connected!");
                  WiFi.reconnect();
//                  WiFi.disconnect();
//                  delay(300);
//                  WiFi.mode(WIFI_STA);
                  
              //    ESP.restart();
                }
          }
  
  if (MDNS.begin(http_name.c_str())) {
    Serial.println("MDNS responder started");
  }
    
 Serial.println(F("wifi ready"));
 
  cws();
  server.begin();
  
  Serial.println(F("Server started"));

  Serial.println(F("WiFi connected"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  // set notification call-back function
  sntp_set_time_sync_notification_cb( timeavailable );

  /**
   * NTP server address could be aquired via DHCP,
   *
   * NOTE: This call should be made BEFORE esp32 aquires IP address via DHCP,
   * otherwise SNTP option 42 would be rejected by default.
   * NOTE: configTime() function call if made AFTER DHCP-client run
   * will OVERRIDE aquired NTP server address
   */
//  sntp_servermode_dhcp(1);    // (optional)

  /**
   * This will set configured ntp servers and constant TimeZone/daylightOffset
   * should be OK if your time zone does not need to adjust daylightOffset twice a year,
   * in such a case time adjustment won't be handled automagicaly.
   */
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer3, ntpServer1, ntpServer2);

  /**
   * A more convenient approach to handle TimeZones with daylightOffset 
   * would be to specify a environmnet variable with TimeZone definition including daylight adjustmnet rules.
   * A list of rules for your zone could be obtained from https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
   */
  //configTzTime(time_zone, ntpServer1, ntpServer2);



//a4 c1 38 4c 3b 4b - Ярс
  bmac[0][0] = 0xA4;
  bmac[0][1] = 0xC1;
  bmac[0][2] = 0x38;
  bmac[0][3] = 0x4C;
  bmac[0][4] = 0x3B;
  bmac[0][5] = 0x4B;
  smac[0]="Комната Ярса";

//a4 c1 38 63 73 72 - балкон  
  bmac[1][0] = 0xA4;
  bmac[1][1] = 0xC1;
  bmac[1][2] = 0x38;
  bmac[1][3] = 0x63;
  bmac[1][4] = 0x73;
  bmac[1][5] = 0x72;
  smac[1]="Балкон";

//a4 c1 38 9e aa 07 - кухня 
  bmac[2][0] = 0xA4;
  bmac[2][1] = 0xC1;
  bmac[2][2] = 0x38;
  bmac[2][3] = 0x9E;
  bmac[2][4] = 0xAA;
  bmac[2][5] = 0x07;
  smac[2]="Кухня <span id='cd'>60</span>";

//a4 c1 38 4c c9 7b - большая комната
  bmac[3][0] = 0xA4;
  bmac[3][1] = 0xC1;
  bmac[3][2] = 0x38;
  bmac[3][3] = 0x4C;
  bmac[3][4] = 0xC9;
  bmac[3][5] = 0x7B;
  smac[3]="Большая комната";
  
  Serial.println("Scanning...");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setInterval(625); // default 100
  pBLEScan->setWindow(625);  // default 100, less or equal setInterval value
  pBLEScan->setActiveScan(true);

  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  pinMode(PowerRelay, OUTPUT);
  digitalWrite(PowerRelay, HIGH);
  delay(500);
  digitalWrite(PowerRelay, LOW);

}

void loop() {
  long start_time = millis();
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  //Serial.print("Devices found: ");
  //Serial.println(foundDevices.getCount());
  //Serial.println("Scan done!");
  pBLEScan->stop();
  pBLEScan->clearResults();
  
    sBLEdata="";
    
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;
    int days = hr / 24;
    
    static char tt[40]; //String tt="";
    sprintf(tt, "Uptime: %03d:%02d:%02d:%02d", days, hr%24, min % 60, sec % 60);

    sBLEdata = tt;

//    Serial.printf("\nUptime: %03d:%02d:%02d:%02d\n", days, hr%24, min % 60, sec % 60);

      //добавить текущее время

  struct tm time;
   
  if(getLocalTime(&time)){
      char buffer[80];
      strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M:%S", &time);
      sBLEdata += ", upd at: " + String(buffer);
  }

Serial.println(sBLEdata);

sBLEdata += "<br>";

sBLEdata += "<table style='border-collapse: collapse; width: 50%;' border='3'><tbody>";

sBLEdata += R"=="==(
<tr>
<td style="width: auto; text-align: center; background-color: #ffff99;">Place</td>
<td style="width: auto; text-align: center; background-color: #ffff99;">Temp</td>
<td style="width: auto; text-align: center; background-color: #ffff99;">Humi</td>
<td style="width: auto; text-align: center; background-color: #ffff99;">vBatt</td>
<td style="width: auto; text-align: center; background-color: #ffff99;">%</td>
<td style="width: auto; text-align: center; background-color: #ffff99;">Ct</td>
<td style="width: auto; text-align: center; background-color: #ffff99;">MAC</td>
</tr>
)=="==";
 
  for(int i=0; i < max_ct; i++){
    if(memcmp(mass, amac[i], 6) == 0){break;}

     Serial.printf("MAC: ");
     printBuffer(amac[i], 6);
          int j=0;
     String NamePlace = "";
     for(j = 0; j < max_ct; j++){
      if(memcmp(amac[i], bmac[j], 6) == 0){
        NamePlace = smac[j];
        break;
       }
     }
     Serial.println(NamePlace);

sBLEdata += "<tr>";
sBLEdata += "<td style=\"width: auto; text-align: center; background-color: #ffff99;\">" + NamePlace + "</td>";
sBLEdata += "<td style=\"width: auto; text-align: center;\">" + String(atemp[i], 2) + "&deg;</td>";
sBLEdata += "<td style=\"width: auto; text-align: center;\">" + String(humi[i], 2) + "%</td>";
sBLEdata += "<td style=\"width: auto; text-align: center; background-color: #" + vcolor(pbatt[i]) + ";\">" + String(vbatt[i], 3) + "</td>";
sBLEdata += "<td style=\"width: auto; text-align: center; background-color: #" + vcolor(pbatt[i]) + ";\">" + String(pbatt[i]) + "</td>";
sBLEdata += "<td style=\"width: auto; text-align: center;\">" + String(mac_ct[i]) + "</td>";
sBLEdata += "<td style=\"width: auto; text-align: center;\">" + sprintBuffer(amac[i], 6) + "</td>";
sBLEdata += "</tr>";
     
//     
//     sBLEdata += sprintBuffer(amac[i], 6);
//
//     sBLEdata += " " + NamePlace;

//     static char ty[100];
//     sprintf(ty, " Temp: %.2f°, Humidity: %.2f%%, Vbatt: %.3f В (%d%%) Ct: %d%<br>", atemp[i], humi[i], vbatt[i], pbatt[i], mac_ct[i]);
//     sBLEdata += ty;
     
     Serial.printf("Temp: %.2f°, Humidity: %.2f%%, Vbatt: %.3fV, Battery: %d%% Ct: %d%\n", atemp[i], humi[i], vbatt[i], pbatt[i], mac_ct[i]);
     mac_ct[i] = 0;

    }

    sBLEdata += "</tbody></table>";
    // sBLEdata += rsr1 + "<br>" + rsr2 + "<br>";

    Serial.println();
    Serial.print("WiFi connected: ");
    Serial.print(WiFi.SSID());
    Serial.print(" ");
    Serial.print(WiFi.localIP());
    Serial.print(" DNS name: ");
    Serial.println(http_name);
//    Serial.println(sBLEdata);
//    Serial.println(sBLEdata.length());

        while(millis() - start_time < 60001) 
        {
          long start_time1 = millis();
          while(millis() - start_time1 < 1000){
            server.handleClient();
         }
         Serial.print(".");
        }
        Serial.println();
        
        Serial.println("Resetting WDT...");
        esp_task_wdt_reset();
}



void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
  rt_rdy = true;
  Serial.println("Got time adjustment from NTP!");
  printLocalTime();
}

// Function that gets current epoch time
time_t getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

void toggleLED(void * parameter){
  pinMode(2, OUTPUT);
  for(;;){ // infinite loop
   
    digitalWrite(2, HIGH);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    digitalWrite(2, LOW);
    // Pause the task for 500ms
    vTaskDelay(2990 / portTICK_PERIOD_MS);

  }
}

String vcolor(int batt_percent){
  if(batt_percent > 50) {
    return "00ff00"; //GREEN
  }
    
  if(batt_percent > 40) {
    return "ffff00"; //YELLOW
  }
    
    return "ff0000"; //RED
 
}

String print_reset_reason(int reason)
{
  String rsr;
  switch ( reason)
  {
    case 1 : rsr="POWERON_RESET";break;          /**<1,  Vbat power on reset*/
    case 3 : rsr="SW_RESET";break;               /**<3,  Software reset digital core*/
    case 4 : rsr="OWDT_RESET";break;             /**<4,  Legacy watch dog reset digital core*/
    case 5 : rsr="DEEPSLEEP_RESET";break;        /**<5,  Deep Sleep reset digital core*/
    case 6 : rsr="SDIO_RESET";break;             /**<6,  Reset by SLC module, reset digital core*/
    case 7 : rsr="TG0WDT_SYS_RESET";break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8 : rsr="TG1WDT_SYS_RESET";break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9 : rsr="RTCWDT_SYS_RESET";break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10 : rsr="INTRUSION_RESET";break;       /**<10, Instrusion tested to reset CPU*/
    case 11 : rsr="TGWDT_CPU_RESET";break;       /**<11, Time Group reset CPU*/
    case 12 : rsr="SW_CPU_RESET";break;          /**<12, Software reset CPU*/
    case 13 : rsr="RTCWDT_CPU_RESET";break;      /**<13, RTC Watch dog Reset CPU*/
    case 14 : rsr="EXT_CPU_RESET";break;         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : rsr="RTCWDT_BROWN_OUT_RESET";break;/**<15, Reset when the vdd voltage is not stable*/
    case 16 : rsr="RTCWDT_RTC_RESET";break;      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : rsr="NO_MEAN";
  }
  return rsr;
}

String verbose_print_reset_reason(int reason)
{
  String vrsr;
  switch ( reason)
  {
    case 1  : vrsr="Vbat power on reset";break;
    case 3  : vrsr="Software reset digital core";break;
    case 4  : vrsr="Legacy watch dog reset digital core";break;
    case 5  : vrsr="Deep Sleep reset digital core";break;
    case 6  : vrsr="Reset by SLC module, reset digital core";break;
    case 7  : vrsr="Timer Group0 Watch dog reset digital core";break;
    case 8  : vrsr="Timer Group1 Watch dog reset digital core";break;
    case 9  : vrsr="RTC Watch dog Reset digital core";break;
    case 10 : vrsr="Instrusion tested to reset CPU";break;
    case 11 : vrsr="Time Group reset CPU";break;
    case 12 : vrsr="Software reset CPU";break;
    case 13 : vrsr="RTC Watch dog Reset CPU";break;
    case 14 : vrsr="for APP CPU, reseted by PRO CPU";break;
    case 15 : vrsr="Reset when the vdd voltage is not stable";break;
    case 16 : vrsr="RTC Watch dog reset digital core and rtc module";break;
    default : vrsr="NO_MEAN";
  }
  return vrsr;
}
