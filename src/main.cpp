#include "main.h"

const char *ssid0 = "DWR-921-327C";
const char *password0 = "xcJwUsHt";

const char *ssid1 = "SFR_6AE0";
const char *password1 = "tjys2kki43ajgb2prv76";

const char *ssid2 = "Blacktop";
const char *password2 = "black1234";

bool greenLedState = false;
const int greenLedPin = 2;
const int redLedPin = 15;
const int buttonPin = 16;
bool buzzerflag = false;
bool loopFlag = true;

char packetBuffer[255];
unsigned int localPort = 9999;

int prevTime;
int prevTimebuzz;

WiFiUDP udp;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiMulti wifiMulti;

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
#ifdef DEBUG
  Serial.println("Connected to AP successfully!");
#endif
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
#ifdef DEBUG
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
#ifdef DEBUG
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
#endif
  while (wifiMulti.run(WiFi_timeout) != WL_CONNECTED)
  {
#ifdef DEBUG
    Serial.println("WiFi not connected!!");
#endif
    delay(500);
    WiFi.disconnect();
    ESP.restart();
  }
}

void notifyClients()
{
  ws.textAll(String(loopFlag));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    if (strcmp((char *)data, "toggle") == 0)
    {
      loopFlag = !loopFlag;
      if (ID == 1)
      {
        analogWrite(greenLedPin, 0);
      }
      else
      {
        if (loopFlag)
        {
          digitalWrite(greenLedPin, false);
          digitalWrite(redLedPin, false);
        }
        else
        {
          digitalWrite(greenLedPin, true);
          digitalWrite(redLedPin, false);
        }
      }
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
#ifdef DEBUG
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
#endif
    break;
  case WS_EVT_DISCONNECT:
#ifdef DEBUG
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
#endif
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String &var)
{
#ifdef DEBUG
  Serial.println(var);
#endif
  if (var == "STATE")
  {
    if (loopFlag)
    {
      return "ON";
    }
    else
    {
      return "OFF";
    }
  }
  return String();
}

void setup()
{
  EEPROM.begin(EEPROM_SIZE);
  ID = EEPROM.read(0);
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println(ID);
#endif

  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  digitalWrite(greenLedPin, greenLedState);
  digitalWrite(redLedPin, greenLedState);
  // Adding all WiFi credential to WiFi multi class
  wifiMulti.addAP(ssid0, password0);
  wifiMulti.addAP(ssid1, password1);
  wifiMulti.addAP(ssid2, password2);
  // Connect to Wi-Fi
  WiFi.onEvent(WiFiStationConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  while (wifiMulti.run(WiFi_timeout) != WL_CONNECTED)
  {
    delay(500);
    WiFi.disconnect();
    ESP.restart();
  }

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html, processor); });

  // Start ElegantOTA
  AsyncElegantOTA.begin(&server);
  // Start server
  server.begin();

  udp.begin(localPort);
}

void receiveUDP()
{
  int len = udp.read(packetBuffer, 255);
  if (len > 0)
  {
    packetBuffer[len] = 0;
#ifdef DEBUG
    Serial.printf("Data : %s\n", packetBuffer);
#endif
    if (strcmp(packetBuffer, "press") == 0)
    {
      if (ID == 1)
      {
        buzzerflag = true;
      }
      else
      {
        greenLedState = false;
        digitalWrite(greenLedPin, greenLedState);
        digitalWrite(redLedPin, greenLedState);
      }
#ifdef DEBUG
      Serial.println(greenLedState);
#endif
    }
  }
}

void loop()
{
  ws.cleanupClients();
  if (loopFlag)
  {
    int packetSize = udp.parsePacket();

    if (packetSize > 0)
    {
#ifdef DEBUG
      Serial.print(" Received packet from : ");
      Serial.println(udp.remoteIP());
      Serial.print(" Size : ");
      Serial.println(packetSize);
#endif
      receiveUDP();
    }

    if (ID == 1)
    {
      if (buzzerflag && (millis() - prevTimebuzz > 500))
      {
        greenLedState = !greenLedState;
        if (greenLedState)
          analogWrite(greenLedPin, 80);
        else
          analogWrite(greenLedPin, 0);
        prevTimebuzz = millis();
      }
      if (digitalRead(buttonPin) == LOW && (millis() - prevTime > 1000))
      {
        udp.beginPacket("192.168.1.43", localPort);
        udp.printf("press");
        udp.endPacket();
        greenLedState = false;
        buzzerflag = false;
        analogWrite(greenLedPin, 0);

#ifdef DEBUG
        Serial.println("button press");
#endif
        prevTime = millis();
      }
    }
    else
    {
      if (digitalRead(buttonPin) == LOW && (millis() - prevTime > 1000))
      {
        udp.beginPacket("192.168.1.27", localPort);
        udp.printf("press");
        udp.endPacket();
        greenLedState = true;
        digitalWrite(greenLedPin, greenLedState);
        digitalWrite(redLedPin, greenLedState);
#ifdef DEBUG
        Serial.println("button press");
#endif
        prevTime = millis();
      }
    }
  }
}