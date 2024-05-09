#include <Arduino.h>
#include <WiFiManager.h>       // Include the WiFiManager library
#include <ESPAsyncWebServer.h> // Include the ESPAsyncWebServer library
#include "keys.h"

AsyncWebServer server(80); // Create a web server on port 80
#define BUTTON_PRESS_DELAY_MS 300
#define OPERATE_PIN 12
#define OPEN_STATUS_PIN 27
#define CLOSE_STATUS_PIN 14
#define OPEN_DOOR_TIME_MS 8000

const char *setup_ssid = "Main Garage rolling door WIFI";

// Define your set of valid keys
const char *validKeys[] = {
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
};

bool isKeyValid(const char *key)
{
  for (int i = 0; i < sizeof(validKeys) / sizeof(validKeys[0]); i++)
  {
    if (strcmp(key, validKeys[i]) == 0)
    {
      return true;
    }
  }
  return false;
}

bool authorized(AsyncWebServerRequest *request)
{
  return !(!request->hasParam("key") || !isKeyValid(request->getParam("key")->value().c_str()));
}

void send_signal(uint8_t pin)
{
  digitalWrite(pin, HIGH);
  delay(BUTTON_PRESS_DELAY_MS);
  digitalWrite(pin, LOW);
}

int get_signal(uint8_t pin)
{
  int count_high = 0;
  int count_low = 0;

  for (int i = 0; i < 100; i++)
  {
    if (digitalRead(pin) == HIGH)
    {
      count_high++;
    }
    else
    {
      count_low++;
    }
    delay(1);
  }

  if (count_high > count_low)
  {
    return HIGH;
  }
  else
  {
    return LOW;
  }
}

bool is_open()
{
  return get_signal(OPEN_STATUS_PIN) == HIGH && get_signal(CLOSE_STATUS_PIN) == LOW;
}

bool is_closed()
{
  return get_signal(OPEN_STATUS_PIN) == LOW && get_signal(CLOSE_STATUS_PIN) == HIGH;
}

bool get_is_flashing()
{
  int open_state = get_signal(OPEN_STATUS_PIN);
  int close_state = get_signal(CLOSE_STATUS_PIN);

  // If OPEN_STATUS_PIN and CLOSE_STATUS_PIN are not in the same state, the door is not flashing.
  if (open_state != close_state)
    return false;

  // Poll OPEN_STATUS_PIN and CLOSE_STATUS_PIN 4 seconds.
  // If they change their state, the door is flashing.
  int initial_state = open_state;
  unsigned long start_time = millis();
  while (millis() - start_time < 1200)
  {
    if (get_signal(OPEN_STATUS_PIN) != initial_state || get_signal(CLOSE_STATUS_PIN) != initial_state)
    {
      return true;
    }
  }
  return false;
}

void setupWiFi()
{
  WiFiManager wifiManager;

  // Start the ESP32 in access point mode and start the configuration portal
  if (!wifiManager.autoConnect(setup_ssid))
  {
    // If the ESP32 failed to connect and hit the timeout, reset and try again
    delay(3000);
    ESP.restart();
  }

  // If you get here, the ESP32 has connected to the WiFi network
}

void handleClose(AsyncWebServerRequest *request)
{
  if (!authorized(request))
    return;

  if (is_open())
  {
    send_signal(OPERATE_PIN);
    request->send(200, "text/plain", "Door is closing");
  }
  else
  {
    request->send(200, "text/plain", "Door is already closed");
  }
}
void handleOpen(AsyncWebServerRequest *request)
{
  if (!authorized(request))
    return;

  if (get_signal(OPEN_STATUS_PIN) == LOW && get_signal(CLOSE_STATUS_PIN) == HIGH)
  {
    send_signal(OPERATE_PIN);
    request->send(200, "text/plain", "Door is opening");
  }
  else
  {
    request->send(200, "text/plain", "Door is already open");
  }
}

void handleOperate(AsyncWebServerRequest *request)
{
  if (!authorized(request))
    return;

  send_signal(OPERATE_PIN);
  request->send(200, "text/plain", "Sent operate command");
}

void handleStatus(AsyncWebServerRequest *request)
{
  if (!authorized(request))
    return;

  bool is_opened = get_signal(OPEN_STATUS_PIN);
  bool is_closed = get_signal(CLOSE_STATUS_PIN);
  bool is_flashing = get_is_flashing();
  unsigned long uptime = millis() / 1000;
  unsigned long seconds = uptime % 60;
  unsigned long minutes = (uptime / 60) % 60;
  unsigned long hours = (uptime / 3600) % 24;
  unsigned long days = uptime / 86400;

  char response[200];
  sprintf(response, "{\"opened\": %d,\n\"closed\": %d,\n\"flashing\": %d,\n\"uptime\": \"%lu days, %lu hours, %lu minutes, %lu seconds\"}", is_opened, is_closed, is_flashing, days, hours, minutes, seconds);
  request->send(200, "text/plain", response);
}

void setup()
{
  setupWiFi();

  pinMode(OPEN_STATUS_PIN, INPUT);
  pinMode(CLOSE_STATUS_PIN, INPUT);

  digitalWrite(OPERATE_PIN, LOW);
  pinMode(OPERATE_PIN, OUTPUT);

  // Define endpoints.
  server.on("/open", HTTP_GET, handleOpen);
  server.on("/close", HTTP_GET, handleClose);
  server.on("/operate", HTTP_GET, handleOperate);
  server.on("/status", HTTP_GET, handleStatus);

  // Start the server
  server.begin();
}

void loop()
{
  // Your main code here
}