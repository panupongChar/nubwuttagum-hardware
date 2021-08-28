#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

#define tckMs portTICK_PERIOD_MS

const char* ssid = "G8's Hotspot";
const char* password = "SegmentationFault"; //change to better word

const char* urlStatus = "http://158.108.182.9:3000/update_customer/0"; 
const char* urlSyncCus = "http://158.108.182.9:50003/syncCus?storeId=0";//change URL later when thing settle down

const int _size = 2 * JSON_OBJECT_SIZE(3); //fix number in arg

StaticJsonDocument<_size> JSONGet; //template
StaticJsonDocument<_size> JSONPost; //template

static TaskHandle_t taskCheck = NULL; //template
static TaskHandle_t taskPost = NULL;
static TaskHandle_t taskLCD = NULL;
static TaskHandle_t taskAlarm = NULL;

/* assign sensor port no. here */
int lcdColumns = 16;
int lcdRows = 2;
int ledR1 = 13;
int ledR2 = 18;
int ledG1 = 14;
int ledG2 = 19;
int pir1 = 27;
int pir2 = 23;
int buzzer = 12;
int channel = 0;
int freq = 400;
int resolution = 8; 

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows); 

/* declare global variable here */
volatile int people = 0;
volatile int limit = 10;
int status_val = 0;
int state = 0;
bool printWait = false;
bool sendWait = false;
bool alarmNow = false;
int alarmCount = 20;
char str[100]; //string is for post stringify json

void WiFi_Connect(){ //for connect WiFi
  WiFi.disconnect();
  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");
  Serial.print("IP Address : ");
  Serial.println(WiFi.localIP());
}

void alarm(void *param){
  while(1){
    if(alarmNow){
      ledcWriteTone(channel,400);
      vTaskDelay(100/tckMs);
      ledcWriteTone(channel,200);
      vTaskDelay(100/tckMs);
      ledcWriteTone(channel,0);
      alarmCount--;
      if(alarmCount==0){
        alarmNow = false;
      }
    }
    vTaskDelay(10/tckMs);
  }
}

void checker(void *param){
  while(1){
    if(state==0){
      if(digitalRead(pir1)){
        state=1;
        digitalWrite(ledR1,HIGH);
        digitalWrite(ledR2,HIGH);
        if(people>=limit){
          digitalWrite(ledG1,HIGH);
        }
        else{
          digitalWrite(ledG1,LOW);
        }
        digitalWrite(ledG2,LOW);
      }
      else if(digitalRead(pir2)){
        state=3;
        digitalWrite(ledR1,HIGH);
        digitalWrite(ledR2,HIGH);
        if(people>=limit){
          digitalWrite(ledG1,HIGH);
        }
        else{
          digitalWrite(ledG1,LOW);
        }
        digitalWrite(ledG2,LOW);
      }
    }
    if(state==1){
      if(digitalRead(pir2)){
        status_val = 1;
        people++;
        printWait = true;
        sendWait = true;
        alarmCount = 20;
        state=2;
        if(people>limit){
          alarmNow=true;
        }
        else{
          ledcWriteTone(channel,480);
          vTaskDelay(500/tckMs);
          ledcWriteTone(channel,370);
          vTaskDelay(500/tckMs);
          ledcWriteTone(channel,0);
        }
        vTaskDelay(5000/portTICK_PERIOD_MS);
        state=0;
        if(people>=limit){
          digitalWrite(ledR1,HIGH);
        }
        else{
          digitalWrite(ledR1,LOW);
        }
        digitalWrite(ledR2,LOW);
        digitalWrite(ledG1,HIGH);
        digitalWrite(ledG2,HIGH);
      }
      else if(!digitalRead(pir1)){
        vTaskDelay(5000/portTICK_PERIOD_MS);
        state=0;
        if(people>=limit){
          digitalWrite(ledR1,HIGH);
        }
        else{
          digitalWrite(ledR1,LOW);
        }
        digitalWrite(ledR2,LOW);
        digitalWrite(ledG1,HIGH);
        digitalWrite(ledG2,HIGH);
      }
    }
    else if(state==3){
      if(digitalRead(pir1)){
        status_val = 0;
        if(people>0){
          people--;
        }
        printWait = true;
        sendWait = true;
        state=4;
        vTaskDelay(5000/portTICK_PERIOD_MS);
        state=0;
        if(people>=limit){
          digitalWrite(ledR1,HIGH);
        }
        else{
          digitalWrite(ledR1,LOW);
        }
        digitalWrite(ledR2,LOW);
        digitalWrite(ledG1,HIGH);
        digitalWrite(ledG2,HIGH);
      }
      else if(!digitalRead(pir2)){
        vTaskDelay(5000/portTICK_PERIOD_MS);
        state=0;
        if(people>=limit){
          digitalWrite(ledR1,HIGH);
        }
        else{
          digitalWrite(ledR1,LOW);
        }
        digitalWrite(ledR2,LOW);
        digitalWrite(ledG1,HIGH);
        digitalWrite(ledG2,HIGH);
      }
    }
    vTaskDelay(10/tckMs);
  }
}

void post(void *param){
  while(1){
      if(sendWait&&(state==2||state==4)){
        if(WiFi.status()==WL_CONNECTED){
          HTTPClient http;
          http.begin(urlStatus);
          http.addHeader("Content-Type", "application/json");
          if(state==2){
            JSONPost["statusId"] = "1";
            Serial.println("Walk in");
          }
          else if(state==4){
            JSONPost["statusId"] = "0";
            Serial.println("Walk out");
          }
          sendWait = false;
          serializeJson(JSONPost, str);
          int httpCode = http.POST(str);
          if(httpCode==HTTP_CODE_OK){
            String payload = http.getString();
            Serial.println(httpCode);
            Serial.println(payload);
          }
          else{
            Serial.println(httpCode);
            Serial.println("ERROR on HTTP Request");
          }
        }
        else{
          WiFi_Connect();
        }
    }
    vTaskDelay(10/tckMs);
  }
}

//void _get(void *param){
//  while(1){
//    if(WiFi.status() == WL_CONNECTED){
//      if(sendWait&&(state==2||state==4)){
//        HTTPClient http;
//        if(status_val){
//          http.begin(url1);
//          Serial.println("Walk in");
//        }
//        else{
//          http.begin(url0);
//          Serial.println("Walk out");
//        }
//        int httpCode = http.GET();
//        if(httpCode==HTTP_CODE_OK){
//          String payload = http.getString();
//          DeserializationError err = deserializeJson(JSONGet, payload);
//          if(err){
//            Serial.print(F("deserializeJson() failed with code "));
//            Serial.println(err.c_str());
//          }
//          else{
//            Serial.println(httpCode);
//            sendWait=false;
//          }
//        }
//        else{
//          Serial.println(httpCode);
//          Serial.println("ERROR on HTTP Request");
//        }
//      }
//    }
//    else{
//      WiFi_Connect();
//    }
//    vTaskDelay(10/tckMs);
//  }
//}

void lcd_update(void *param){
  while(1){
    if(printWait&&(state==2||state==4)){
      lcd.clear(); 
      lcd.setCursor(3, 0);
      if(people<limit){
        lcd.print("Welcome!");
      }
      else{
        lcd.print("Please Wait!");
      }
      lcd.setCursor(0,1);
      lcd.print("People : ");
      lcd.setCursor(9,1);
      lcd.print(people);
      lcd.setCursor(12,1);
      lcd.print("/");
      lcd.setCursor(13,1);
      lcd.print(limit);
      printWait = false;
    }
    vTaskDelay(10/tckMs);
  }
}

void setup() {
//  assign sensor input or output here
  pinMode(pir1, INPUT);
  pinMode(pir2, INPUT);
  pinMode(ledR1, OUTPUT);
  pinMode(ledG1, OUTPUT);
  pinMode(ledR2, OUTPUT);
  pinMode(ledG2, OUTPUT);
  digitalWrite(ledR1,LOW);
  digitalWrite(ledR2,LOW);
  digitalWrite(ledG1,HIGH);
  digitalWrite(ledG2,HIGH);
  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  lcd.clear(); 
  lcd.setCursor(1, 0);
  lcd.print("Setting...");
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(buzzer, channel);
  Serial.begin(115200); //baud rate is parameter , baud is about rate of sending serial monitor
  delay(4000);
  WiFi.mode(WIFI_STA);
  WiFi_Connect();
  //synchronize the people in store and limit people of store
  HTTPClient http;
  http.begin(urlSyncCus);
  int httpCode = http.GET();
  while(httpCode!=HTTP_CODE_OK){
    Serial.println(httpCode);
    Serial.println("ERROR on HTTP Request");
  }
  String payload = http.getString();
  DeserializationError err = deserializeJson(JSONGet, payload);
  if(err){
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
  }
  else{
    Serial.println("Synchronization's done!");
    people = JSONGet["currentCustomer"];
    limit = JSONGet["maxCustomer"];
  }
  //print initialized amount of people in store
  lcd.clear(); 
  lcd.setCursor(3, 0);
  lcd.print("Welcome!");
  lcd.setCursor(0,1);
  lcd.print("People : ");
  lcd.setCursor(9,1);
  lcd.print(people);
  lcd.setCursor(12,1);
  lcd.print("/");
  lcd.setCursor(13,1);
  lcd.print(limit);
  if(people>limit){
    digitalWrite(ledR1,HIGH);
  }
//xTaskCreatePinnedToCore(task_name,"name",memory(bytes),NULL,priority,TaskHandle_t,Core0or1);
  xTaskCreatePinnedToCore(checker, "PIRchecker", 1024 * 32, NULL, 1, &taskCheck, 0);
  xTaskCreatePinnedToCore(alarm, "alarm", 1024 * 32, NULL, 2, &taskAlarm, 0);
  xTaskCreatePinnedToCore(lcd_update, "lcd_update", 1024 * 32, NULL, 1, &taskLCD, 1);
  xTaskCreatePinnedToCore(post, "post", 1024 * 32, NULL, 2, &taskPost, 1);
  Serial.println("Standby!");
}

void loop() { //loop is run on core 1 with priority 1 also should have taskdelay
}
