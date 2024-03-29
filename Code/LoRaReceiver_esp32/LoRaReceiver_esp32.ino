#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <SPI.h>
#include "SPIFFS.h"
#include <LoRa.h>

#define ss 5
#define rst 14
#define dio0 2

AsyncWebServer server(80);


const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "ip";
const char *PARAM_INPUT_4 = "gateway";
const char *PARAM_INPUT_5 = "subnet";
const char *PARAM_INPUT_6 = "username";

String ssid;
String pass;
String ip;
String gateway;
String netmask;
String username;

const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *ipPath = "/ip.txt";
const char *gatewayPath = "/gateway.txt";
const char *subnetPath = "/subnet.txt";
const char *usernamePath = "/username.txt";

IPAddress localIP;
IPAddress localGateway;
IPAddress localSubnet;

String WIFI_hostname = "ESPGateway_1";
char Server_domain[] = "jgs.pnxelec.com";
int Server_port = 80;
SocketIOclient socketIO;
#define USE_SERIAL Serial

unsigned long previousMillis = 0;
const long interval = 10000; // 와이파이 연결 대기 시간

void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case sIOtype_DISCONNECT:
    USE_SERIAL.printf("[IOc] Disconnected!\n");
    break;
  case sIOtype_CONNECT:
    USE_SERIAL.printf("[IOc] Connected to url: %s\n", payload);

    // join default namespace (no auto join in Socket.IO V3)
    socketIO.send(sIOtype_CONNECT, "/");
    break;
  case sIOtype_EVENT:
  {
    char *sptr = NULL;
    int id = strtol((char *)payload, &sptr, 10);
    USE_SERIAL.printf("[IOc] get event: %s id: %d\n", payload, id);
    if (id)
    {
      payload = (uint8_t *)sptr;
    }
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error)
    {
      USE_SERIAL.print(F("deserializeJson() failed: "));
      USE_SERIAL.println(error.c_str());
      return;
    }

    String eventName = doc[0];
    USE_SERIAL.printf("[IOc] event name: %s\n", eventName.c_str());

    // Message Includes a ID for a ACK (callback)
    if (id)
    {
      // creat JSON message for Socket.IO (ack)
      DynamicJsonDocument docOut(1024);
      JsonArray array = docOut.to<JsonArray>();

      // add payload (parameters) for the ack (callback function)
      JsonObject param1 = array.createNestedObject();
      param1["now"] = millis();

      // JSON to String (serializion)
      String output;
      output += id;
      serializeJson(docOut, output);

      // Send event
      socketIO.send(sIOtype_ACK, output);
    }
  }
  break;
  case sIOtype_ACK:
    USE_SERIAL.printf("[IOc] get ack: %u\n", length);
    break;
  case sIOtype_ERROR:
    USE_SERIAL.printf("[IOc] get error: %u\n", length);
    break;
  case sIOtype_BINARY_EVENT:
    USE_SERIAL.printf("[IOc] get binary: %u\n", length);
    break;
  case sIOtype_BINARY_ACK:
    USE_SERIAL.printf("[IOc] get binary ack: %u\n", length);
    break;
  }
}

void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    USE_SERIAL.println("An error has occurred while mounting SPIFFS");
  }
  USE_SERIAL.println("SPIFFS mounted successfully");
}

String readFile(fs::FS &fs, const char *path)
{
  USE_SERIAL.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory())
  {
    USE_SERIAL.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  USE_SERIAL.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    USE_SERIAL.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    USE_SERIAL.println("- file written");
  }
  else
  {
    USE_SERIAL.println("- frite failed");
  }
}

bool initWiFi()
{
  WiFi.mode(WIFI_STA);
  USE_SERIAL.println(WIFI_hostname);
  if (ssid == "")
  {
    USE_SERIAL.println("Undefined SSID.");
    return false;
  }

  if (ip == "" || gateway == "")
  {
    USE_SERIAL.println("IP and Gateway not defined.DHCP");
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }
  else
  {
    USE_SERIAL.println("Static IP defined.");
    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    localSubnet.fromString(netmask.c_str());
    WiFi.config(localIP, localGateway, localSubnet);
  }

  if (!WiFi.setHostname(WIFI_hostname.c_str()))
  {
    USE_SERIAL.println("Hostname failed to configure");
  }
  else
  {
    USE_SERIAL.println("Hostname configure");
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  USE_SERIAL.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      USE_SERIAL.println("Failed to connect.");
      return false;
    }
  }

  USE_SERIAL.println(WiFi.localIP());
  return true;
}

void SettingPage()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/wifimanager.html", "text/html"); });

  server.serveStatic("/", SPIFFS, "/");

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        // HTTP POST ssid value
        if (p->name() == PARAM_INPUT_1) {
          ssid = p->value().c_str();
          USE_SERIAL.print("SSID set to: ");
          USE_SERIAL.println(ssid);
          // Write file to save value
          writeFile(SPIFFS, ssidPath, ssid.c_str());
        }
        // HTTP POST pass value
        if (p->name() == PARAM_INPUT_2) {
          pass = p->value().c_str();
          USE_SERIAL.print("Password set to: ");
          USE_SERIAL.println(pass);
          // Write file to save value
          writeFile(SPIFFS, passPath, pass.c_str());
        }
        // HTTP POST ip value
        if (p->name() == PARAM_INPUT_3) {
          ip = p->value().c_str();
          USE_SERIAL.print("IP Address set to: ");
          USE_SERIAL.println(ip);
          // Write file to save value
          writeFile(SPIFFS, ipPath, ip.c_str());
        }
        // HTTP POST gateway value
        if (p->name() == PARAM_INPUT_4) {
          gateway = p->value().c_str();
          USE_SERIAL.print("Gateway set to: ");
          USE_SERIAL.println(gateway);
          // Write file to save value
          writeFile(SPIFFS, gatewayPath, gateway.c_str());
        }
        // HTTP POST subnet value
        if (p->name() == PARAM_INPUT_5) {
          netmask = p->value().c_str();
          USE_SERIAL.print("Subnet set to: ");
          USE_SERIAL.println(netmask);
          // Write file to save value
          writeFile(SPIFFS, subnetPath, netmask.c_str());
        }
        // HTTP POST subnet value
        if (p->name() == PARAM_INPUT_6) {
          username = p->value().c_str();
          USE_SERIAL.print("Username set to: ");
          USE_SERIAL.println(username);
          // Write file to save value
          writeFile(SPIFFS, usernamePath, username.c_str());
        }

        //USE_SERIAL.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
    delay(3000);
    ESP.restart(); });
  server.begin();
}

void setup()
{

  Serial.begin(115200);

  LoRa.setPins(ss, rst, dio0);

  while (!LoRa.begin(433E6)) {
    USE_SERIAL.println(".");
    delay(500);
  }

  LoRa.setSyncWord(0xF3);
  USE_SERIAL.println("LoRa Initializing OK!");

  USE_SERIAL.setDebugOutput(true);
  initSPIFFS();
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  ip = readFile(SPIFFS, ipPath);
  gateway = readFile(SPIFFS, gatewayPath);
  netmask = readFile(SPIFFS, subnetPath);
  username = readFile(SPIFFS, usernamePath);

  USE_SERIAL.println(ssid);
  USE_SERIAL.println(pass);
  USE_SERIAL.println(ip);
  USE_SERIAL.println(gateway);
  USE_SERIAL.println(netmask);
  USE_SERIAL.println(username);

  if (initWiFi())
  {
    SettingPage();
  }
  else
  {
    // Connect to Wi-Fi network with SSID and password
    USE_SERIAL.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESPGateway_1", NULL);

    IPAddress IP = WiFi.softAPIP();
    USE_SERIAL.print("AP IP address: ");
    USE_SERIAL.println(IP);

    SettingPage();
  }
  socketIO.begin(Server_domain, Server_port, "/socket.io/?EIO=4");
  socketIO.onEvent(socketIOEvent);
}

void alert(String user_id, int device_no)
{
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  array.add("Alert"); // event name
  JsonObject jsondata = array.createNestedObject();
  jsondata["user_id"] = user_id;
  jsondata["device_no"] = device_no;
  String JSONdata;
  serializeJson(doc, JSONdata);
  socketIO.sendEVENT(JSONdata);
  USE_SERIAL.println(JSONdata);
  return;
}

void test(String user_id, int device_no)
{
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  array.add("test_func"); // event name
  JsonObject jsondata = array.createNestedObject();
  jsondata["user_id"] = user_id;
  jsondata["device_no"] = device_no;
  String JSONdata;
  serializeJson(doc, JSONdata);
  socketIO.sendEVENT(JSONdata);
  USE_SERIAL.println(JSONdata);
  return;
} 

void loop()
{
  socketIO.loop();
  //USE_SERIAL.print("HELL");
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String LoRaData;
    int device_no = 0;
    int stat = 0;
    // received a packet
    USE_SERIAL.print("Received packet '");

    // read packet
    while (LoRa.available()) {
      LoRaData += LoRa.readString();
      USE_SERIAL.print(LoRaData); 
    }
    for(int i = 0;i<5;i++){
      device_no *= 10;
      device_no += LoRaData[3+i] - '0';
    }
    USE_SERIAL.println(device_no);
    stat = LoRaData[9] - '0';
    USE_SERIAL.println(stat);
    // print RSSI of packet
    USE_SERIAL.print("' with RSSI ");
    USE_SERIAL.println(LoRa.packetRssi());
    
    if(stat == 1)
    {
      alert(username,device_no);
    }
    else if(stat == 0)
    {
      test(username,device_no);
    }
  }
}
