#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <StreamString.h>
#include <BlynkSimpleEsp8266.h>
#include <WS2812FX.h>

#define BLYNK_PRINT Serial
#define LED_COUNT 125
#define LED_PIN D3

#define MyApiKey "" // Api da sinric
#define MyWifiSSID "" // Nome do Wifi
#define MyWifiPassword ""// Senha do Wifi
#define MyBlynkAuth ""// Auth da Blynk
#define MyLedStripId ""//Id da light, pela Sinric
#define HEARTBEAT_INTERVAL 300000
#define TIMER_MS 10000

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;
WS2812FX fita = WS2812FX(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);
WS2812FX fita_notification = WS2812FX(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);
WS2812FX fita_real = WS2812FX(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

uint64_t heartbeatTimestamp = 0;
unsigned long last_change = 0;
unsigned long now = 0;
bool isConnected = false;
bool notificationStatus = false;

void imprimeCor(int r, int g, int b){
  Serial.println((String)"\nRed:"+r+"\nGreen:"+g+"\nBlue:"+b);
}

void onOff(String deviceId, bool statusLed) {
  if (deviceId == MyLedStripId){
    if (statusLed) {
      fita.start();
      Blynk.virtualWrite(V7, true);
    }else{
      fita.stop();
    }
    Blynk.virtualWrite(V4, statusLed);
  }   
}

void colorAbsolute(String deviceId, int decimalColor){
  if (deviceId == MyLedStripId){  
    String hexstring =  String(decimalColor, HEX);
    hexstring = "#" + hexstring;
    int number = (int) strtol( &hexstring[1], NULL, 16);
    int r = number >> 16;
    int g = number >> 8 & 0xFF;
    int b = number & 0xFF;
    fita.setColor(g, r, b);
    Blynk.virtualWrite(V4, true);
    Blynk.virtualWrite(V5, r, g, b);
    fita.start();
  }
}

void brightnessAbsolute(String deviceId, int brilho){
  if (deviceId == MyLedStripId){
    brilho = map(brilho, 0, 100, 0, 255);
    fita.setBrightness(brilho);
    Blynk.virtualWrite(V3, brilho);
  }
}

String formatHEX(String cor){
  if(cor.length() == 0){
    cor = "00"; 
  }
  else if(cor.length()==1){
    cor = "0" + cor;
  }
  return cor;
}

String convertRGBtoHEX(int r, int g, int b){
  String redStr =  formatHEX(String(r, HEX));
  String greenStr =  formatHEX(String(g, HEX));
  String blueStr =  formatHEX(String(b, HEX));
  String hexStr = redStr + greenStr + blueStr;
  hexStr.toUpperCase();
  return hexStr;
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      isConnected = false;    
      Serial.printf("[WSc] Webservice disconnected from sinric.com!\n");
      break;
    case WStype_CONNECTED: {
      isConnected = true;
      Serial.printf("[WSc] Service connected to sinric.com at url: %s\n", payload);
      Serial.printf("Waiting for commands from sinric.com ...\n");        
      }
      break;
    case WStype_TEXT: {
        Serial.printf("[WSc] get text: %s\n", payload);

#if ARDUINOJSON_VERSION_MAJOR == 5
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
        DynamicJsonDocument json(1024);
        deserializeJson(json, (char*) payload);      
#endif        
        String deviceId = json ["deviceId"];     
        String action = json ["action"];
        
        if(action == "action.devices.commands.OnOff") { // Switch 
          String value = json ["value"]["on"];
          onOff(deviceId, value == "true");
        }
        else if (action  == "action.devices.commands.ColorAbsolute") {
          String value = json ["value"]["color"]["spectrumRGB"];
          colorAbsolute(deviceId, value.toInt());
        }
        else if (action  == "action.devices.commands.BrightnessAbsolute") {
          String value = json ["value"]["brightness"];
          brightnessAbsolute(deviceId, value.toInt());
        }
        else if (action == "test") {
          Serial.println("Recebendo o comando de teste da sinric.com");
        }
      }
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
    default: break;
  }
}

void myCustomShow(void){
  for (int i=0; i<fita_real.numPixels(); i++){
    if(fita_notification.getPixelColor(i) == BLACK){
      fita_real.setPixelColor(i, fita.getPixelColor(i));
    }else{
      fita_real.setPixelColor(i, fita_notification.getPixelColor(i));
    }
  }
  fita_real.Adafruit_NeoPixel::show();
}

void setupWifi(){
  WiFiMulti.addAP(MyWifiSSID, MyWifiPassword);
  Serial.println();
  Serial.print("Conectando ao Wifi: ");
  Serial.println(MyWifiSSID);  

  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if(WiFiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("WiFi conectado. ");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  }
}

void setupWebSocket(){
  webSocket.begin("iot.sinric.com", 80, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", MyApiKey);
  webSocket.setReconnectInterval(5000);
}

void setupWs2812fx(){
  fita.init();
  fita.setBrightness(255);
  fita.setSpeed(200);
  fita.setColor(255, 255, 255);
  fita.setMode(FX_MODE_STATIC);
  fita.start();

  fita_notification.init();
  fita_notification.setBrightness(255);
  fita_notification.setSpeed(3000);
  fita_notification.setColor(BLACK);
  fita_notification.setMode(FX_MODE_COMET);
  fita_notification.start();
  
  fita_real.init();
  fita_real.setBrightness(255);
  fita_real.setSegment(0,0,LED_COUNT - 1 , FX_MODE_STATIC, BLACK, 1000 , NO_OPTIONS);
  fita_real.start ();
  fita_real.service ();

  fita.setCustomShow(myCustomShow);
  fita_notification.setCustomShow(myCustomShow);
}

void setupBlynk(){
  Blynk.begin(MyBlynkAuth, MyWifiSSID, MyWifiPassword);
}

void setup() {
  Serial.begin(9600);
  setupWifi();
  setupWs2812fx();
  setupBlynk();
  setupWebSocket();
}

void loopWs2812fx(){
  fita.service();
  fita_notification.service();
}

void loop() {
  Blynk.run();
  webSocket.loop();
  loopWs2812fx();

  if(isConnected) {
      uint64_t now = millis();
     if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
          heartbeatTimestamp = now;
          webSocket.sendTXT("H");          
     }
  }
  now = millis();
  if(now - last_change > TIMER_MS && notificationStatus) {
    Serial.println("Notificação acabou");
    fita_notification.setColor(0,0,0);
    last_change = now;
    notificationStatus = false;
  }
}
BLYNK_WRITE(V0) {//Notification
  Serial.println("Notificação");
  int r = param[0].asInt();
  int g = param[1].asInt();
  int b = param[2].asInt();
  fita_notification.setColor(g, r, b);
  notificationStatus = true;
  last_change = millis();
  Serial.println("Notificação iniciada");
}

BLYNK_WRITE(V1) {//Speed
  fita.setSpeed(param.asInt()*10);
}

BLYNK_WRITE(V2) {//Efeitos
  fita.setMode(param.asInt() - 1);
}

BLYNK_WRITE(V3) {//Brilho
  fita.setBrightness(param.asInt());
}

BLYNK_WRITE(V4) {//OnOff
  onOff(MyLedStripId, param.asInt());
}

BLYNK_WRITE(V5) {//Cor GRB
  int r = param[0].asInt();
  int g = param[1].asInt();
  int b = param[2].asInt();
  fita.setColor(g, r, b);
  Blynk.virtualWrite(V6, convertRGBtoHEX(r, g, b));
}

BLYNK_WRITE(V6){//Color HEX
  String hexstring = param.asStr();
  hexstring = "#" + hexstring;  
  int number = (int) strtol( &hexstring[1], NULL, 16);
  int r = number >> 16;
  int g = number >> 8 & 0xFF;
  int b = number & 0xFF;
  
  fita.setColor(g, r, b);
  Blynk.virtualWrite(V5, r, g, b);
}

BLYNK_WRITE(V7) {//Pause/Running
  if (param.asInt()) {
    fita.resume();
    Blynk.virtualWrite(V4, true);
  }else{
    fita.pause();
  }
}
