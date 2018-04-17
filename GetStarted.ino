// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.
// To get started please visit https://microsoft.github.io/azure-iot-developer-kit/docs/projects/connect-iot-hub?utm_source=ArduinoExtension&utm_medium=ReleaseNote&utm_campaign=VSCode
#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "lib.h"

#include "config.h"
#include "utility.h"

#define REPORT_HEATING_TEMPLATE "{\"__heatingOn\": %s}"
#define REPORT_USAGE_TEMPLATE "{\"__beingUsed\": %s}"

static bool hasWifi = false;
int messageCount = 1;

int buttonAStatus = HIGH;
int buttonBStatus = HIGH;

bool beInUsed = false;
bool heating = false;
bool needHeating = false;

char startUsingTime[25], endUsingTime[25], startHeatingTime[25], endHeatingTime[25];

void use()
{
  STRING_HANDLE data = STRING_new();
  if (beInUsed == false)
  {
    beInUsed = true;
    Screen.print(2, "Being Used... \r\n");
    getCurrentTime(startUsingTime);
    STRING_sprintf(data, REPORT_USAGE_TEMPLATE, "true");
    Serial.println(STRING_c_str(data));
    DevKitMQTTClient_SendReportedStatus(data);
    STRING_delete(data);
  }
  else
  {
    beInUsed = false;
    getCurrentTime(endUsingTime);
    Screen.print(2, "Available \r\n");
    STRING_sprintf(data, REPORT_USAGE_TEMPLATE, "false");
    DevKitMQTTClient_SendReportedStatus(data);
    Serial.println(STRING_c_str(data));
    STRING_delete(data);
    sendMessage("usage", startUsingTime, endUsingTime);
  }
}

void heat()
{
  STRING_HANDLE data = STRING_new();
  if (heating == false)
  {
    heating = true;
    getCurrentTime(startHeatingTime);
    lightOn();
    Screen.print(3, " > Heating...");
    STRING_sprintf(data, REPORT_HEATING_TEMPLATE, "true");
    Serial.println(STRING_c_str(data));
    DevKitMQTTClient_SendReportedStatus(data);
    STRING_delete(data);
  }
  else
  {
    heating = false;
    lightOff();
    Screen.print(3, " > Ready to heat");
    getCurrentTime(endHeatingTime);
    STRING_sprintf(data, REPORT_HEATING_TEMPLATE, "false");
    Serial.println(STRING_c_str(data));
    DevKitMQTTClient_SendReportedStatus(data);
    STRING_delete(data);
    sendMessage("heating", startHeatingTime, endHeatingTime);
  }
}

static void InitWifi()
{
  Screen.print(2, "Connecting...");

  if (WiFi.begin() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    hasWifi = true;
    Screen.print(2, "Start... \r\n");
  }
  else
  {
    hasWifi = false;
    Screen.print(1, "No Wi-Fi\r\n ");
  }
}

static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    Serial.println("Cannot malloc variable to store device twin\n");
    return;
  }
  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  Serial.println("====twin===\n");
  bool heatingOn = parseTwinMessage(updateState, temp);
  Serial.println(temp);

  if ((heatingOn && !heating) || (!heatingOn && heating))
  {
    needHeating = true;
  }

  free(temp);
}

void setup()
{
  pinMode(USER_BUTTON_A, INPUT);
  pinMode(USER_BUTTON_B, INPUT);

  Screen.init();
  Screen.print(0, "Toilet Simulator");
  Screen.print(2, "Initializing...");

  Screen.print(3, " > Serial");
  Serial.begin(115200);

  Screen.print(3, " > WiFi");
  hasWifi = false;
  InitWifi();
  if (!hasWifi)
  {
    return;
  }

  Screen.print(3, " > Sensors");
  SensorInit();

  Screen.print(3, " > ...");
  DevKitMQTTClient_Init(true);

  DevKitMQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);

  Screen.print(1, "A:Heat B:Use");
  Screen.print(2, "Available \r\n");
  Screen.print(3, " > Ready to Heat \r\n");
}

void buttonStatusDetect()
{
  if (digitalRead(USER_BUTTON_A) != buttonAStatus)
  {
    if (buttonAStatus == HIGH)
    {
      Serial.println("Button A (Heat toilet) pressed.");
      heat();
    }
    buttonAStatus = digitalRead(USER_BUTTON_A);
  }

  if (digitalRead(USER_BUTTON_B) != buttonBStatus)
  {
    if (buttonBStatus == HIGH)
    {
      Serial.println("Button B (Use toilet) pressed.");
      use();
    }
    buttonBStatus = digitalRead(USER_BUTTON_B);
  }
}

void sendMessage(char *type, char *startTime, char *endTime)
{
  char messagePayload[MESSAGE_MAX_LEN];
  bool temperatureAlert = readMessage(messageCount++, messagePayload);
  EVENT_INSTANCE *message = DevKitMQTTClient_Event_Generate(messagePayload, MESSAGE);

  DevKitMQTTClient_Event_AddProp(message, "type", type);
  DevKitMQTTClient_Event_AddProp(message, "startTime", startTime);
  DevKitMQTTClient_Event_AddProp(message, "endTime", endTime);

  DevKitMQTTClient_SendEventInstance(message);
}

void getCurrentTime(char *timeBuffer)
{
  struct tm *currentTime;
  time_t rawtime;
  time(&rawtime);
  currentTime = localtime(&rawtime);
  strftime(timeBuffer, 25, "%F %X", currentTime);
  Serial.println(timeBuffer);
}

void loop()
{
  if (hasWifi)
  {
    buttonStatusDetect();

    if (needHeating)
    {
      heat();
      needHeating = false;
    }
    DevKitMQTTClient_Check();
  }
  delay(100);
}
