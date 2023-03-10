#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <virtuabotixRTC.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>
#include "SPIFFS.h"

Preferences preferences;

//const char* ssid = "Inteligentny ogród";
//const char* password = "SmartGarden";

const char* ssid = "najlepsza wifi na osce 2.4G";
const char* password = "studentpiwo";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

Adafruit_BME280 hBME280;
bool BME280_Status = false;
float BME280_Temperature = 0.0;
float BME280_Humidity = 0.0;

Adafruit_BMP280 hBMP280;
bool BMP280_Status = false;
float BMP280_Temperature = 0.0;

BH1750 hBH1750;
bool BH1750_Status = false;
unsigned int BH1750_Light = 0;
const unsigned int BH1750_LightDusk = 20;

const unsigned int HCSR501_PORT = 13;
bool HCSR501_Motion = false;

const unsigned int GREENHOUSE_PORT = 12;
const unsigned int GREENHOUSE_CH = 0;
const unsigned int GREENHOUSE_RES = 10;
const unsigned int GREENHOUSE_FREQ = 2000;
bool gh_auto_mode = false;
bool gh_manual_mode = false;
bool gh_manual_state = true;
bool gh_temp_off_iS = false;
unsigned int gh_target_temp = 0;
unsigned int gh_temp_off = 0;

float PID_current_time = 0.0;
float PID_previous_error = 0.0;
float integral = 0.0;

const unsigned int LED1_PORT = 32;
unsigned int LED1_State = HIGH;
bool l1_auto_mode = false;
bool l1_manual_mode = false;
bool l1_manual_state = true;
bool l1_time_off_iS = false;
unsigned int l1_time_off_hour = 0;
unsigned int l1_time_off_min = 0;
unsigned int l1_time_on_hour = 0;
unsigned int l1_time_on_min = 0;

const unsigned int LED2_PORT = 33;
unsigned int LED2_State = HIGH;
bool l2_auto_mode = false;
bool l2_manual_mode = false;
bool l2_manual_state = true;
unsigned int l2_detect_mode = 0;

const unsigned int WATERPUMP_PORT = 14;
bool wp_auto_mode = false;
bool wp_manual_mode = false;
bool wp_manual_state = false;
bool wp_duration_iS = false;
unsigned int wp_duration = 0;
unsigned int wp_humidity = 0;
unsigned int wp_duration_sec = 0;
unsigned int wp_duration_day = 0;

unsigned long currentMillis = 0;
unsigned long previousMillis_peroid1 = 0;
unsigned long previousMillis_peroid2 = 0;
const unsigned long period1 = 100;
const unsigned long period2 = 1000;

virtuabotixRTC RTC(18, 19, 23);

void setup()
{
  Serial.begin(115200);
  Serial.println("--- INIT TEST ---");
  initPorts();
  initFS();
  //initAP();
  initWiFi();
  initSensors();
  initSettingsData();
  initWebServerSocket();
  initRestoreData();
}

void loop()
{
  currentMillis = millis();
  if (currentMillis - previousMillis_peroid1 >= period1)
  {
    readSensorsValues();
    operateGreenHouse();
    if (currentMillis - previousMillis_peroid2 >= period2)
    {
      operateLed1();
      operateLed2();
      operateWaterPump();
      webSocketNotify(JSONInformationValues());
      previousMillis_peroid2 = millis();
    }
    previousMillis_peroid1 = millis();
  }
}

void initPorts()
{
  pinMode(LED1_PORT, OUTPUT);
  pinMode(LED2_PORT, OUTPUT);
  pinMode(WATERPUMP_PORT, OUTPUT);
  ledcSetup(GREENHOUSE_CH, GREENHOUSE_FREQ, GREENHOUSE_RES);
  ledcAttachPin(GREENHOUSE_PORT, GREENHOUSE_CH);
  ledcWrite(GREENHOUSE_CH, 0);
  digitalWrite(LED1_PORT, HIGH);
  digitalWrite(LED2_PORT, HIGH);
  digitalWrite(WATERPUMP_PORT, HIGH);
  Serial.println("Ports: OK");
}

void initFS()
{
  if (SPIFFS.begin())
  {
    Serial.println("SPIFFS: OK");
  }
  else
  {
    Serial.println("SPIFFS: FAILED");
  }
}

void initAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("WiFi: ");
  Serial.println(WiFi.softAPIP());
}

void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }
  Serial.print("WiFi: ");
  Serial.println(WiFi.localIP());
}

void initSensors()
{
  BME280_Status = hBME280.begin(0x76);
  BMP280_Status = hBMP280.begin(0x77);
  BH1750_Status = hBH1750.begin();
  pinMode(HCSR501_PORT, INPUT);
  if (BME280_Status)
  {
    Serial.println("BME280: OK");
  }
  else
  {
    Serial.println("BME280: FAILED");
  }
  if (BMP280_Status)
  {
    Serial.println("BMP280: OK");
  }
  else {
    Serial.println("BMP280: FAILED");
  }
  if (BH1750_Status)
  {
    Serial.println("BH1750: OK");
  }
  else
  {
    Serial.println("BH1750: FAILED");
  }
  Serial.println("HC-SR501: OK");
}

void initSettingsData()
{
  preferences.begin("SmartGarden", false);
  // --- GreenHouse ---
  gh_auto_mode = preferences.getBool("gh_auto_mode", false);
  gh_manual_mode = preferences.getBool("gh_manual_mode", false);
  gh_manual_state = preferences.getBool("gh_manual_state", true);
  gh_temp_off_iS = preferences.getBool("gh_temp_off_iS", false);
  gh_target_temp = preferences.getUInt("gh_target_temp", 0);
  gh_temp_off = preferences.getUInt("gh_temp_off", 0);
  // --- LED 1 ---
  LED1_State = preferences.getUInt("LED1_State", HIGH);
  l1_auto_mode = preferences.getBool("l1_auto_mode", false);
  l1_manual_mode = preferences.getBool("l1_manual_mode", false);
  l1_manual_state = preferences.getBool("l1_manual_state", true);
  l1_time_off_iS = preferences.getBool("l1_time_off_iS", false);
  l1_time_off_hour = preferences.getUInt("l1_off_hour", 0);
  l1_time_off_min = preferences.getUInt("l1_off_min", 0);
  l1_time_on_hour = preferences.getUInt("l1_on_hour", 0);
  l1_time_on_min = preferences.getUInt("l1_on_min", 0);
  // --- LED 2 ---
  LED2_State = preferences.getUInt("LED2_State", HIGH);
  l2_auto_mode = preferences.getBool("l2_auto_mode", false);
  l2_manual_mode = preferences.getBool("l2_manual_mode", false);
  l2_manual_state = preferences.getBool("l2_manual_state", true);
  l2_detect_mode = preferences.getUInt("l2_detect_mode", false);
  // --- WaterPump ---
  wp_auto_mode = preferences.getBool("wp_auto_mode", false);
  wp_manual_mode = preferences.getBool("wp_manual_mode", false);
  wp_manual_state = preferences.getBool("wp_manual_state", false);
  wp_duration_iS = preferences.getBool("wp_duration_iS", false);
  wp_duration = preferences.getUInt("wp_duration", 0);
  wp_humidity = preferences.getUInt("wp_humidity", 0);
  wp_duration_sec = preferences.getUInt("wp_duration_sec", 0);
  wp_duration_day = preferences.getUInt("wp_duration_day", 0);
  Serial.println("Settings: OK");
}

void initWebServerSocket()
{
  ws.onEvent(webSocketOnEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    request->send(SPIFFS, "/index.html", "text/html");
  });
  server.serveStatic("/", SPIFFS, "/");
  server.begin();
  Serial.println("Server: OK");
}

void initRestoreData()
{
  previousMillis_peroid1 = millis();
  previousMillis_peroid2 = millis();
  digitalWrite(LED1_PORT, LED1_State);
  digitalWrite(LED2_PORT, LED2_State);
  Serial.println("Data: OK");
  Serial.println();
}

void webSocketOnEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_DATA)
  {
    webSocketOnMessage(arg, data, len);
  }
}

void webSocketOnMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    if (strcmp((char*)data, "getValues") == 0)
    {
      webSocketNotify(JSONGreenHouseValues());
      webSocketNotify(JSONLedsValues());
      webSocketNotify(JSONWaterPumpValues());
      webSocketNotify(JSONInformationValues());
    }
    else
    {
      StaticJsonDocument<400> jsonData;
      DeserializationError error = deserializeJson(jsonData, (char*)data);
      if (!error)
      {
        if (jsonData.containsKey("cfg"))
        {
          const char* cfgType = jsonData["cfg"];
          if (strcmp(cfgType, "time") == 0)
          {
            int t_second = jsonData["second"];
            int t_minute = jsonData["minute"];
            int t_hour = jsonData["hour"];
            int t_day = jsonData["day"];
            int t_day_of_month = jsonData["day_of_month"];
            int t_month = jsonData["month"];
            int t_year = jsonData["year"];
            RTC.setDS1302Time(t_second, t_minute, t_hour, t_day, t_day_of_month, t_month, t_year);
            Serial.println("Zaktualizowano czas");
          }
          else if (strcmp(cfgType, "greenhouse") == 0)
          {
            gh_auto_mode = jsonData["gh_auto_mode"];
            if (gh_auto_mode)
            {
              gh_target_temp = jsonData["gh_target_temp"];
              gh_temp_off_iS = jsonData["gh_temp_off_is_set"];
              if (gh_temp_off_iS)
              {
                gh_temp_off = jsonData["gh_temp_off"];
              }
              else
              {
                gh_temp_off = 0;
              }
              gh_manual_mode = false;
              gh_manual_state = false;
            }
            else
            {
              gh_manual_mode = jsonData["gh_manual_mode"];
              if (gh_manual_mode)
              {
                gh_manual_state = jsonData["gh_manual_state"];
              }
              else
              {
                gh_manual_state = false;
              }
              gh_temp_off_iS = false;
              gh_target_temp = 0;
              gh_temp_off = 0;
            }
            preferences.putBool("gh_auto_mode", gh_auto_mode);
            preferences.putBool("gh_manual_mode", gh_manual_mode);
            preferences.putBool("gh_manual_state", gh_manual_state);
            preferences.putBool("gh_temp_off_iS", gh_temp_off_iS);
            preferences.putUInt("gh_target_temp", gh_target_temp);
            preferences.putUInt("gh_temp_off", gh_temp_off);
            integral = 0.0;
            PID_previous_error = 0.0;
            PID_current_time = 0.0;
          }
          else if (strcmp(cfgType, "led") == 0)
          {
            l1_auto_mode = jsonData["l1_auto_mode"];
            if (l1_auto_mode)
            {
              l1_time_on_hour = jsonData["l1_time_on_hour"];
              l1_time_on_min = jsonData["l1_time_on_min"];
              l1_time_off_iS = jsonData["l1_time_off_is_set"];
              if (l1_time_off_iS)
              {
                l1_time_off_hour = jsonData["l1_time_off_hour"];
                l1_time_off_min = jsonData["l1_time_off_min"];
              }
              else
              {
                l1_time_off_hour = 0;
                l1_time_off_min = 0;
              }
              l1_manual_mode = false;
              l1_manual_state = false;
            }
            else
            {
              l1_manual_mode = jsonData["l1_manual_mode"];
              if (l1_manual_mode)
              {
                l1_manual_state = jsonData["l1_manual_state"];
              }
              else
              {
                l1_manual_state = false;
              }
              l1_time_off_iS = false;
              l1_time_off_hour = 0;
              l1_time_off_min = 0;
              l1_time_on_hour = 0;
              l1_time_on_min = 0;
            }
            l2_auto_mode = jsonData["l2_auto_mode"];
            if (l2_auto_mode)
            {
              l2_detect_mode = jsonData["l2_detect_mode"];
              l2_manual_mode = false;
              l2_manual_state = false;
            }
            else
            {
              l2_manual_mode = jsonData["l2_manual_mode"];
              if (l2_manual_mode)
              {
                l2_manual_state = jsonData["l2_manual_state"];
              }
              else
              {
                l2_manual_state = false;
              }
              l2_detect_mode = 0;
            }
            preferences.putBool("l1_auto_mode", l1_auto_mode);
            preferences.putBool("l1_manual_mode", l1_manual_mode);
            preferences.putBool("l1_manual_state", l1_manual_state);
            preferences.putBool("l1_time_off_iS", l1_time_off_iS);
            preferences.putUInt("l1_off_hour", l1_time_off_hour);
            preferences.putUInt("l1_off_min", l1_time_off_min);
            preferences.putUInt("l1_on_hour", l1_time_on_hour);
            preferences.putUInt("l1_on_min", l1_time_on_min);
            preferences.putBool("l2_auto_mode", l2_auto_mode);
            preferences.putBool("l2_manual_mode", l2_manual_mode);
            preferences.putBool("l2_manual_state", l2_manual_state);
            preferences.putUInt("l2_detect_mode", l2_detect_mode);
          }
          else if (strcmp(cfgType, "water_pump") == 0)
          {
            wp_auto_mode = jsonData["wp_auto_mode"];
            if (wp_auto_mode)
            {
              wp_humidity = jsonData["wp_humidity_below"];
              wp_duration_iS = jsonData["wp_duration_time_is_set"];
              if (wp_duration_iS)
              {
                wp_duration = jsonData["wp_duration_time"];
              }
              else
              {
                wp_duration = 0;
              }
              wp_manual_mode = false;
              wp_manual_state = false;
            }
            else
            {
              wp_manual_mode = jsonData["wp_manual_mode"];
              if (wp_manual_mode)
              {
                wp_manual_state = jsonData["wp_manual_state"];
              }
              else
              {
                wp_manual_state = false;
              }
              wp_duration_iS = false;
              wp_duration = 0;
              wp_humidity = 0;
            }
            preferences.putBool("wp_auto_mode", wp_auto_mode);
            preferences.putBool("wp_manual_mode", wp_manual_mode);
            preferences.putBool("wp_manual_state", wp_manual_state);
            preferences.putBool("wp_duration_iS", wp_duration_iS);
            preferences.putUInt("wp_duration", wp_duration);
            preferences.putUInt("wp_humidity", wp_humidity);
          }
        }
      }
    }
  }
}

void webSocketNotify(String values)
{
  ws.textAll(values);
}

void readSensorsValues()
{
  if (BME280_Status)
  {
    BME280_Temperature = hBME280.readTemperature();
    BME280_Humidity = hBME280.readHumidity();
  }
  if (BMP280_Status)
  {
    BMP280_Temperature = hBMP280.readTemperature();
  }
  if (BH1750_Status)
  {
    BH1750_Light = hBH1750.readLightLevel();
  }
  if (digitalRead(HCSR501_PORT) == HIGH)
  {
    HCSR501_Motion = true;
  }
  else
  {
    HCSR501_Motion = false;
  }
}

void operateGreenHouse()
{
  if (gh_auto_mode)
  {
    if (gh_temp_off_iS && gh_temp_off <= BME280_Temperature)
    {
      if (ledcRead(GREENHOUSE_CH) != 0)
      {
        ledcWrite(GREENHOUSE_CH, 0);
      }
    }
    else
    {
      float PWM_Duty = 1023.0 * PID_TemperatureControler(gh_target_temp, BMP280_Temperature);
      if (PWM_Duty < 0.0)
      {
        PWM_Duty = 0.0;
        Serial.print("WINDUPA dolna");
      }
      else if (PWM_Duty > 1023.0)
      {
        PWM_Duty = 1023.0;
        Serial.print("WINDUPA górna");
      }
      unsigned int iPWM_Duty = round(PWM_Duty);
      ledcWrite(GREENHOUSE_CH, iPWM_Duty);
      Serial.print(PID_current_time);
      Serial.print(";");
      Serial.print(iPWM_Duty);
      Serial.print(";");
      Serial.println(BMP280_Temperature);
      PID_current_time += 0.1;
    }
  }
  else if (gh_manual_mode && gh_manual_state)
  {
    Serial.println(BMP280_Temperature);
    PID_current_time += 0.1;
    if (ledcRead(GREENHOUSE_CH) != 1023)
    {
      ledcWrite(GREENHOUSE_CH, 1023);
    }
  }
  else if (ledcRead(GREENHOUSE_CH) != 0)
  {
    ledcWrite(GREENHOUSE_CH, 0);
  }
}

void operateLed1()
{
  if (l1_auto_mode)
  {
    RTC.updateTime();
    int cHour = RTC.hours;
    int cMin = RTC.minutes;
    if (l1_time_off_iS && cHour == l1_time_off_hour && cMin == l1_time_off_min)
    {
      if (digitalRead(LED1_PORT) == LOW)
      {
        LED1_State = HIGH;
        digitalWrite(LED1_PORT, HIGH);
        preferences.putUInt("LED1_State", LED1_State);
      }
    }
    else if (cHour == l1_time_on_hour && cMin == l1_time_on_min)
    {
      if (digitalRead(LED1_PORT) == HIGH)
      {
        LED1_State = LOW;
        digitalWrite(LED1_PORT, LOW);
        preferences.putUInt("LED1_State", LED1_State);
      }
    }
  }
  else if (l1_manual_mode && l1_manual_state)
  {
    if (digitalRead(LED1_PORT) == HIGH)
    {
      LED1_State = LOW;
      digitalWrite(LED1_PORT, LOW);
      preferences.putUInt("LED1_State", LED1_State);
    }
  }
  else if (digitalRead(LED1_PORT) == LOW)
  {
    LED1_State = HIGH;
    digitalWrite(LED1_PORT, HIGH);
    preferences.putUInt("LED1_State", LED1_State);
  }
}

void operateLed2()
{
  if (l2_auto_mode)
  {
    if ((l2_detect_mode == 1 && BH1750_Light <= BH1750_LightDusk) || (l2_detect_mode == 2 && HCSR501_Motion) || (l2_detect_mode == 3 && BH1750_Light <= BH1750_LightDusk && HCSR501_Motion))
    {
      if (digitalRead(LED2_PORT) == HIGH)
      {
        LED2_State = LOW;
        digitalWrite(LED2_PORT, LOW);
        preferences.putUInt("LED2_State", LED2_State);
      }
    }
    else if (digitalRead(LED2_PORT) == LOW)
    {
      LED2_State = HIGH;
      digitalWrite(LED2_PORT, HIGH);
      preferences.putUInt("LED2_State", LED2_State);
    }
  }
  else if (l2_manual_mode && l2_manual_state)
  {
    if (digitalRead(LED2_PORT) == HIGH)
    {
      LED2_State = LOW;
      digitalWrite(LED2_PORT, LOW);
      preferences.putUInt("LED2_State", LED2_State);
    }
  }
  else if (digitalRead(LED2_PORT) == LOW)
  {
    LED2_State = HIGH;
    digitalWrite(LED2_PORT, HIGH);
    preferences.putUInt("LED2_State", LED2_State);
  }
}

void operateWaterPump()
{
  if (wp_auto_mode)
  {
    if (wp_duration_iS && digitalRead(WATERPUMP_PORT) == LOW)
    {
      RTC.updateTime();
      int cDay = RTC.dayofmonth;
      if (cDay == wp_duration_day)
      {
        wp_duration_sec++;
      }
      else
      {
        wp_duration_sec = 0;
        wp_duration_day = cDay;
        preferences.putUInt("wp_duration_day", wp_duration_day);
      }
      preferences.putUInt("wp_duration_sec", wp_duration_sec);
    }
    if (wp_duration_iS && wp_duration * 60 <= wp_duration_sec)
    {
      if (digitalRead(WATERPUMP_PORT) == LOW)
      {
        digitalWrite(WATERPUMP_PORT, HIGH);
      }
    }
    else if (BME280_Humidity <= wp_humidity)
    {
      if (digitalRead(WATERPUMP_PORT) == HIGH)
      {
        digitalWrite(WATERPUMP_PORT, LOW);
      }
    }
    else if (digitalRead(WATERPUMP_PORT) == LOW)
    {
      digitalWrite(WATERPUMP_PORT, HIGH);
    }
  }
  else if (wp_manual_mode && wp_manual_state)
  {
    if (digitalRead(WATERPUMP_PORT) == HIGH)
    {
      digitalWrite(WATERPUMP_PORT, LOW);
    }
  }
  else if (digitalRead(WATERPUMP_PORT) == LOW)
  {
    digitalWrite(WATERPUMP_PORT, HIGH);
  }
}

float PI_TemperatureControler(float setPoint, float measured)
{
  float u_unsat, u_sat, P, I, error;
  float Kp = 0.460098058608956;
  float Ki = 0.00603133974695534;
  float Kb = 10;
  float dt = 0.1;
  float u_max = 1;
  float u_min = 0;

  error = setPoint - measured;
  integral = integral + (error + PID_previous_error) * (dt / 2.0);

  P = Kp * error;
  I = Ki * integral;

  u_unsat = P + I;
  if (u_unsat > u_max)
  {
    u_sat = u_max;
    integral = integral - Kb * (u_unsat - u_max);
  }
  else if (u_unsat < u_min)
  {
    u_sat = u_min;
    integral = integral - Kb * (u_unsat - u_min);
  }
  else
  {
    u_sat = u_unsat;
  }
  PID_previous_error = error;
  return u_sat;
}

float PID_TemperatureControler(float setPoint, float measured)
{
  float u_unsat, u_sat, P, I, D, error, derivative;
  float Kp = 0.362498012950815;
  float Ki = 0.004496409943;
  float Kd = 2.4169596946102;
  float Kb = 3;
  float dt = 0.1;
  float u_max = 1;
  float u_min = 0;

  error = setPoint - measured;
  integral = integral + (error + PID_previous_error) * (dt / 2.0);
  derivative = (error - PID_previous_error)/dt;

  P = Kp * error;
  I = Ki * integral;
  D = Kd * derivative;

  u_unsat = P + I + D;
  if (u_unsat > u_max)
  {
    u_sat = u_max;
    integral = integral - Kb * (u_unsat - u_max);
  }
  else if (u_unsat < u_min)
  {
    u_sat = u_min;
    integral = integral - Kb * (u_unsat - u_min);
  }
  else
  {
    u_sat = u_unsat;
  }
  PID_previous_error = error;
  return u_sat;
}

String JSONGreenHouseValues()
{
  String JSONString;
  StaticJsonDocument<300> GreenHouseValues;
  GreenHouseValues["gh_auto_mode"] = gh_auto_mode;
  GreenHouseValues["gh_manual_mode"] = gh_manual_mode;
  GreenHouseValues["gh_manual_state"] = gh_manual_state;
  GreenHouseValues["gh_target_temp"] = gh_target_temp;
  GreenHouseValues["gh_temp_off_is_set"] = gh_temp_off_iS;
  GreenHouseValues["gh_temp_off"] = gh_temp_off;
  serializeJson(GreenHouseValues, JSONString);
  return JSONString;
}

String JSONLedsValues()
{
  String JSONString;
  StaticJsonDocument<400> LampsValues;
  LampsValues["l1_auto_mode"] = l1_auto_mode;
  LampsValues["l1_manual_mode"] = l1_manual_mode;
  LampsValues["l1_manual_state"] = l1_manual_state;
  LampsValues["l1_time_on_hour"] = l1_time_on_hour;
  LampsValues["l1_time_on_min"] = l1_time_on_min;
  LampsValues["l1_time_off_is_set"] = l1_time_off_iS;
  LampsValues["l1_time_off_hour"] = l1_time_off_hour;
  LampsValues["l1_time_off_min"] = l1_time_off_min;
  LampsValues["l2_auto_mode"] = l2_auto_mode;
  LampsValues["l2_manual_mode"] = l2_manual_mode;
  LampsValues["l2_manual_state"] = l2_manual_state;
  LampsValues["l2_detect_mode"] = l2_detect_mode;
  serializeJson(LampsValues, JSONString);
  return JSONString;
}

String JSONWaterPumpValues()
{
  String JSONString;
  StaticJsonDocument<300> WaterPumpValues;
  WaterPumpValues["wp_auto_mode"] = wp_auto_mode;
  WaterPumpValues["wp_manual_mode"] = wp_manual_mode;
  WaterPumpValues["wp_manual_state"] = wp_manual_state;
  WaterPumpValues["wp_humidity_below"] = wp_humidity;
  WaterPumpValues["wp_duration_time_is_set"] = wp_duration_iS;
  WaterPumpValues["wp_duration_time"] = wp_duration;
  serializeJson(WaterPumpValues, JSONString);
  return JSONString;
}

String JSONInformationValues()
{
  RTC.updateTime();
  char sBuffer[20];
  String JSONString;
  StaticJsonDocument<200> InformationValues;
  snprintf(sBuffer, sizeof sBuffer, "%02d:%02d:%02d", RTC.hours, RTC.minutes, RTC.seconds);
  InformationValues["current-time"] = sBuffer;
  snprintf(sBuffer, sizeof sBuffer, "%0.2f °C", BMP280_Temperature);
  InformationValues["current-greenhouse-temp"] = sBuffer;
  snprintf(sBuffer, sizeof sBuffer, "%d °C", gh_target_temp);
  InformationValues["target-greenhouse-temp"] = sBuffer;
  snprintf(sBuffer, sizeof sBuffer, "%0.2f °C", BME280_Temperature);
  InformationValues["outside-temp"] = sBuffer;
  snprintf(sBuffer, sizeof sBuffer, "%0.2f %%", BME280_Humidity);
  InformationValues["humidity"] = sBuffer;
  if (digitalRead(LED1_PORT) == LOW)
  {
    snprintf(sBuffer, sizeof sBuffer, "Włączona");
  }
  else
  {
    snprintf(sBuffer, sizeof sBuffer, "Wyłączona");
  }
  InformationValues["lamp-1-state"] = sBuffer;
  if (digitalRead(LED2_PORT) == LOW)
  {
    snprintf(sBuffer, sizeof sBuffer, "Włączona");
  }
  else
  {
    snprintf(sBuffer, sizeof sBuffer, "Wyłączona");
  }
  InformationValues["lamp-2-state"] = sBuffer;
  if (digitalRead(WATERPUMP_PORT) == LOW)
  {
    snprintf(sBuffer, sizeof sBuffer, "Włączona");
  }
  else
  {
    snprintf(sBuffer, sizeof sBuffer, "Wyłączona");
  }
  InformationValues["water-pump-state"] = sBuffer;
  serializeJson(InformationValues, JSONString);
  return JSONString;
}
