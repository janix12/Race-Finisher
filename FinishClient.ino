#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

String ProgVer = "2.0";

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RGB_LED_PIN 14
#define LED_BUILTIN 22
#define NUM_LEDS 1
#define VBAT_PIN 33
#define BUTTON_PIN 12
#define VIBRA_PIN 13

volatile bool IsPressed = false;
volatile bool IsFinished = false;
volatile bool IsProcessed = false;
String unitID = "NoDef";
String unitCh = "R?";
float battV = 3.3;
const float alpha = 0.03; 
volatile float last_batt_avg = 3.3;
volatile int RoundCounter = 0;

String Racer_1 = "08:3A:8D:96:40:58";
String Racer_2 = "B0:B2:1C:F8:B5:C4";
String Racer_3 = "B0:B2:1C:F8:BC:48";
String Racer_4 = "B0:B2:1C:F8:AE:88";
String Test_ID = "40:22:D8:07:9C:4C";
String Spare_1 = "FC:B4:67:4E:7A:F0";

uint8_t serverAddress[] = {0x30, 0x30, 0xF9, 0x34, 0x84, 0xE0}; //30:30:F9:34:84:E0
uint8_t serverAddressS3[] = {0x30, 0x30, 0xF9, 0x59, 0xE7, 0xE8}; //30:30:F9:59:E7:E8
esp_now_peer_info_t peerInfo;

Adafruit_NeoPixel ws2812b(NUM_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

typedef struct {
    uint8_t modeNum;
    volatile bool isReset;
} incoming_struct;

incoming_struct serverMessage;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  if(status == ESP_NOW_SEND_SUCCESS){
    Serial.println("Delivery Success");
    IsProcessed = true;
  }
  else{
    Serial.println("Delivery Fail");
  }
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&serverMessage, incomingData, sizeof(serverMessage));
  Serial.print("Packet received --> ");
  bool isReset = serverMessage.isReset;
  if(isReset){
    IsPressed = false;
    IsProcessed = false;
    IsFinished = false;
    RoundCounter = 0;
    Serial.println("Server reset -> OK");
  } 
  else{
    Serial.println("No user message received");
  }
}

void IRAM_ATTR OnButtonPress() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 3000)
  {
    if(!IsProcessed){      
      if(!IsPressed){
        IsPressed = true;
      }
    }
  }
  last_interrupt_time = interrupt_time;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(26,27); //(SDA,SCL)
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VBAT_PIN, INPUT);
  pinMode(VIBRA_PIN, OUTPUT);
  digitalWrite(VIBRA_PIN, LOW);

  serverMessage.modeNum = 0;
  serverMessage.isReset = false;

  WiFi.mode(WIFI_STA);
  Serial.println("Tiny Drones - Race Finisher");
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
  
  if(display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED init OK!");
    DisplayDefault();
  }
  
  unitID = RacerSelector();
  Serial.print("Unit name: ");
  Serial.println(unitID);
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    display.clearDisplay();
    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("ESP-NOW init error!");
    display.display();
    return;
  }
  else{
    esp_now_register_send_cb(OnDataSent);
    
    //AddMacToServer(serverAddress);
    AddMacToServer(serverAddressS3);
    
    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("OnDataRecv -> Registred..");
  }

  //Gomb beállítás
  Serial.print("Init button -->  ");
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), OnButtonPress, FALLING);
  Serial.println("attachInterrupt -> Added");
  
  ws2812b.begin();
  ws2812b.setBrightness(130);
  ws2812b.clear();
  ws2812b.setPixelColor(0, ws2812b.Color(255, 255, 255));
  ws2812b.show();
  delay(2000);
  ws2812b.clear();
  ws2812b.setPixelColor(0, ws2812b.Color(255, 0, 0));
  ws2812b.show();
  digitalWrite(VIBRA_PIN, HIGH);
}

void loop() {
  if(IsPressed)
  {
    digitalWrite(VIBRA_PIN, LOW);
    if(serverMessage.modeNum == 0){
      SendNOW();
      IsPressed = false;
    }
    else{
      RoundCounter++;
      if(RoundCounter >= 3){
        SendNOW();
        IsPressed = false;
      }
      else{
        IsPressed = false;
      }
    }
  }
  else{
    if(IsFinished){
      if(!IsProcessed){
        SendNOW();
      }
      else{
        display.invertDisplay(true);
        DisplaySucces();
        ws2812b.clear(); 
        ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0));
        ws2812b.show();
      }
    }
    else{
      DisplayDefault();
      ws2812b.clear(); 
      ws2812b.setPixelColor(0, ws2812b.Color(255, 0, 0));
      ws2812b.show();
      delay(50);
    }
  }
  delay(180);
  digitalWrite(VIBRA_PIN, HIGH);
}

void SendNOW(){
  String msg = unitID + unitCh;
  esp_err_t result = esp_now_send(0, (uint8_t *) &msg, sizeof(msg));
  if (result == ESP_OK){
    Serial.println("Sent success!");
    DisplaySucces();
    IsFinished = true;
    delay(200);
  }
}

void DisplaySucces(){
    Serial.println("Sent with success");
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(4,25);
    display.setTextColor(SSD1306_WHITE);
    display.println("*FINISHED*"); //display.println("* OK! *");
    display.setTextSize(1);
    display.setCursor(1,53);
    display.println(GetBatteryVoltage());
    display.display();
}

void DisplayDefault(){
    display.clearDisplay();
    display.invertDisplay(false);
    display.setTextSize(1);
    display.setCursor(1,2);
    display.println("Tiny Drones");
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(28,25);
    display.println(unitID);
    display.setTextSize(1);
    display.setCursor(1,52);
    GetVoltage();
    display.print("V");
    display.print(ProgVer);
    display.setCursor(102,52);
    display.print(String(battV,1));
    display.println("V");
    DrawCounter();
    display.display();
}

void DrawCounter(){
  if(serverMessage.modeNum == 1){
    if(RoundCounter == 0){
      display.drawRect(89, 2, 10, 12, SSD1306_WHITE);
      display.drawRect(102, 2, 10, 12, SSD1306_WHITE);
      display.drawRect(115, 2, 10, 12, SSD1306_WHITE);
    }
    if(RoundCounter == 1){
      display.fillRect(89, 2, 10, 12, SSD1306_WHITE);
    }
    if(RoundCounter == 2){
      display.fillRect(89, 2, 10, 12, SSD1306_WHITE);
      display.fillRect(102, 2, 10, 12, SSD1306_WHITE);
    }
    if(RoundCounter == 3){
      display.fillRect(89, 2, 10, 12, SSD1306_WHITE);
      display.fillRect(102, 2, 10, 12, SSD1306_WHITE);
      display.fillRect(115, 2, 10, 12, SSD1306_WHITE);
    }
  }
  else{
      display.drawRect(115, 2, 10, 12, SSD1306_WHITE);
  }
}

String RacerSelector(){
  String mac = WiFi.macAddress();

  if(mac == Racer_1){
    unitCh = " - R1";
    return "Racer 1";
  }
  if(mac == Racer_2){
    unitCh = " - R3";
    return "Racer 2";
  }
  if(mac == Racer_3){
    unitCh = " - R6";
    return "Racer 3";
  }
  if(mac == Racer_4){
    unitCh = " - R7";
    return "Racer 4";
  }
  if(mac == Test_ID){
    unitCh = " - R?";
    return "Test ID";
  }
  if(mac == Spare_1){
    unitCh = " - R?";
    return "Spare 1";
  }
}

String GetBatteryVoltage(){
  String retVal = "";
  GetVoltage();
  retVal += "      Batt: ";
  retVal += String(battV, 1) + "V";
  Serial.println(retVal);
  return retVal;
}

void GetVoltage(){
  float current_volt = ((float)analogRead(VBAT_PIN) / 4095) * 3.3 * 2 * 1.06;
  battV = (current_volt * alpha) + (last_batt_avg * (1-alpha));
  last_batt_avg = battV;
}

void AddMacToServer(uint8_t mac[]){
  // register peer  
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  memcpy(peerInfo.peer_addr, mac, 6); 
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.print("Failed to add peer: ");
    Serial.println(macStr);
  }
  else{
    Serial.print("Peer add succes: ");
    Serial.println(macStr);
  }
}
