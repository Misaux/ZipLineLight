#include "main.h"

/*const char* ssid = "DWR-921-327C";
const char* password = "xcJwUsHt";*/

const char *ssid = "SFR_6AE0";
const char *password = "tjys2kki43ajgb2prv76";

bool greenLedState = false;
const int greenLedPin = 2;
const int redLedPin = 15;
const int buttonPin = 16;
bool buzzerflag = false;

char packetBuffer[255];
unsigned int localPort = 9999;

WiFiUDP udp;
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

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
  WiFi.begin(ssid, password);
}

void notifyClients()
{
  ws.textAll(String(greenLedState));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    if (strcmp((char *)data, "toggle") == 0)
    {
      greenLedState = !greenLedState;
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
    if (greenLedState)
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
// Serial port for debugging purposes
#ifdef DEBUG
  Serial.begin(115200);
#endif
  EEPROM.begin(EEPROM_SIZE);
  ID = EEPROM.read(0);
#ifdef DEBUG
  Serial.println(ID);
#endif

  pinMode(greenLedPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  digitalWrite(greenLedPin, greenLedState);
  digitalWrite(redLedPin, !greenLedPin);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  WiFi.onEvent(WiFiStationConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
#ifdef DEBUG
    Serial.println("Connecting to WiFi..");
#endif
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
      }
#ifdef DEBUG
      Serial.println(greenLedState);
#endif
    }
  }
}

int prevTime;
int prevTimebuzz;

void loop()
{
  ws.cleanupClients();
  digitalWrite(greenLedPin, greenLedState);

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
      digitalWrite(greenLedPin, greenLedState);
      prevTimebuzz = millis();
    }
    if (digitalRead(buttonPin) == LOW && (millis() - prevTime > 1000))
    {
      udp.beginPacket("192.168.1.43", localPort);
      udp.printf("press");
      udp.endPacket();
      greenLedState = false;
      buzzerflag = false;
      digitalWrite(greenLedPin, greenLedState);
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
#ifdef DEBUG
      Serial.println("button press");
#endif
      prevTime = millis();
    }
  }
}