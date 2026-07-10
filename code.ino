#define BLYNK_TEMPLATE_ID "TMPL6zJ6eVK70"
#define BLYNK_TEMPLATE_NAME "Đố ÁN TỐT NGHIỆP"
#define BLYNK_AUTH_TOKEN    "tqc85tfJIUWHT1y6KLF6ARzGifwCtMrQ"
#define BLYNK_PRINT Serial

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include "DHT.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ===== WIFI =====
char ssid[] = "abcd";
char pass[] = "12345678"; 

// ===== PIN =====
#define GAS_PIN   34
#define FAN_PIN   32
#define SERVO_PIN 13
#define RFID_SS   5
#define RFID_RST  16
#define DHTPIN    4
#define DHTTYPE   DHT11
#define LED1 25
#define LED2 26    
#define LED3 27
#define LED4 33   

// ===== DEVICE =====
LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(RFID_SS, RFID_RST);
Servo door;
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// ===== UID =====
String adminUID = "63 47 86 2F";
String uidList[20];
int uidCount = 0;

// ===== FLAGS =====
bool adminMode   = false;
bool addMode     = false;
bool delMode     = false;
bool doorOpen    = false;
bool fanManual   = false;
bool gasAlert    = false;
bool blinkState  = false;
bool systemReady = false;

// ===== TIME =====
unsigned long adminTime    = 0;
unsigned long doorTime     = 0;
unsigned long lcdHoldUntil = 0;
const unsigned long ADMIN_TIMEOUT = 120000;

int blinkTimerID = -1;

// =====================================================
// LCD
// =====================================================

void showLCD(String l1, String l2, unsigned long hold = 0) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);
  if (hold > 0) lcdHoldUntil = millis() + hold;
}

bool lcdHold() { return millis() < lcdHoldUntil; }

// =====================================================
// WIFI + BLYNK
// =====================================================

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  showLCD("DANG NOI", "WIFI...");
  WiFi.begin(ssid, pass);
  unsigned long s = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - s < 15000)
    delay(500);
  bool ok = (WiFi.status() == WL_CONNECTED);
  showLCD(ok ? "WIFI OK" : "WIFI FAIL", "", 1500);
  return ok;
}

bool connectBlynk() {
  showLCD("DANG NOI", "BLYNK...");
  Blynk.config(BLYNK_AUTH_TOKEN, "blynk.cloud", 80);
  bool ok = Blynk.connect(5000);
  showLCD(ok ? "BLYNK OK" : "BLYNK FAIL", "", 1500);
  return ok;
}

void reconnectSystem() {
  bool wifiOK  = (WiFi.status() == WL_CONNECTED);
  bool blynkOK = Blynk.connected();
  if (!wifiOK) {
    systemReady = false;
    showLCD("WIFI MAT KET", "NOI LAI...", 1000);
    wifiOK = connectWiFi();
  }
  if (wifiOK && !blynkOK) {
    systemReady = false;
    blynkOK = connectBlynk();
  }
  if (wifiOK && blynkOK) {
    if (!systemReady) { systemReady = true; showLCD("HE THONG", "SAN SANG", 1500); }
  } else {
    systemReady = false;
    showLCD("CHUA KET NOI", "THU LAI...", 1000);
  }
}

// =====================================================
// UID
// =====================================================

String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

int findUID(String uid) {
  for (int i = 0; i < uidCount; i++)
    if (uidList[i] == uid) return i;
  return -1;
}

// =====================================================
// BLINK LED4
// =====================================================

void blinkLED() {
  blinkState = !blinkState;
  digitalWrite(LED4, blinkState);
}

void startBlink() {
  if (blinkTimerID == -1)
    blinkTimerID = timer.setInterval(300L, blinkLED);
}

void stopBlink() {
  if (blinkTimerID != -1) {
    timer.deleteTimer(blinkTimerID);
    blinkTimerID = -1;
  }
  digitalWrite(LED4, LOW);
}

// =====================================================
// DOOR
// =====================================================

void openDoor() {
  door.write(90);
  doorOpen = true;
  doorTime = millis();
  showLCD("MO CUA", "", 1500);
  if (Blynk.connected()) Blynk.virtualWrite(V4, 1);
}

void closeDoor() {
  if (gasAlert) return;
  door.write(0);
  doorOpen = false;
  showLCD("DONG CUA", "", 1000);
  if (Blynk.connected()) Blynk.virtualWrite(V4, 0);
}

void autoCloseDoor() {
  if (doorOpen && millis() - doorTime > 5000) closeDoor();
}

// =====================================================
// RFID
// =====================================================

void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

  String uid = getUID();
  Serial.println("UID: " + uid);
  if (Blynk.connected()) Blynk.virtualWrite(V11, uid);

  if (uid == adminUID) {
    adminMode = true; adminTime = millis();
    openDoor(); showLCD("ADMIN MODE", "ON", 1500);
    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1(); return;
  }

  if (adminMode && addMode) {
    if (findUID(uid) == -1 && uidCount < 20) {
      uidList[uidCount++] = uid;
      showLCD("DA THEM THE", "", 1500);
      if (Blynk.connected()) Blynk.virtualWrite(V11, "ADD: " + uid);
    }
    addMode = false;
    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1(); return;
  }

  if (adminMode && delMode) {
    int pos = findUID(uid);
    if (pos != -1) {
      for (int i = pos; i < uidCount - 1; i++) uidList[i] = uidList[i + 1];
      uidCount--;
      showLCD("DA XOA THE", "", 1500);
      if (Blynk.connected()) Blynk.virtualWrite(V11, "DELETE: " + uid);
    } else {
      showLCD("KHONG TIM", "THAY THE", 1500);
    }
    delMode = false;
    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1(); return;
  }

  if (findUID(uid) != -1) openDoor();
  else showLCD("THE SAI", "", 1500);
  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
}

// =====================================================
// SENSOR + GUI BLYNK - moi 1s
// =====================================================
void readAndSend() {
  if (!systemReady) {
    showLCD("CHO KET NOI", "...", 0);
    return;
  }

  float t   = dht.readTemperature();
  float h   = dht.readHumidity();
  int   gas = analogRead(GAS_PIN);

  // ===== FAN + GAS ALERT =====
  if (gas > 2000) {
    digitalWrite(FAN_PIN, HIGH);
    if (!gasAlert) {
      gasAlert = true;
      openDoor();
      startBlink();
      showLCD("CANH BAO KHI", "GAS CAO!", 3000);
      Serial.println("GAS ALERT!");
      if (Blynk.connected()) {
        Blynk.logEvent("gas_alert", "CANH BAO: Gas cao! Gia tri: " + String(gas));
        Blynk.virtualWrite(V12, 1);
      }
    }
  } else {
    if (gasAlert) {
      gasAlert = false;
      stopBlink();
      showLCD("KHI GAS", "BINH THUONG", 2000);
      Serial.println("GAS NORMAL");
      if (Blynk.connected()) {
        Blynk.logEvent("gas_alert", "Gas binh thuong. Gia tri: " + String(gas));
        Blynk.virtualWrite(V12, 0);
      }
    }
    if (!fanManual) digitalWrite(FAN_PIN, LOW);
  }

  // ===== GUI BLYNK =====
  if (Blynk.connected()) {
    Blynk.virtualWrite(V6, isnan(t) ? 0 : t);
    Blynk.virtualWrite(V7, isnan(h) ? 0 : h);
    Blynk.virtualWrite(V8, gas);
  }

  // ===== LCD =====
  if (lcdHold()) return;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("G:"); lcd.print(gas); lcd.print("ppm");  
  lcd.print(" T:"); lcd.print(isnan(t) ? "?" : String((int)t)); lcd.write(223); lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("H:"); lcd.print(isnan(h) ? "?" : String((int)h)); lcd.print("%");
  if (gas > 1200) lcd.print(" FAN");
}

// =====================================================
// ADMIN TIMEOUT
// =====================================================

void checkAdminTimeout() {
  if (adminMode && millis() - adminTime > ADMIN_TIMEOUT) {
    adminMode = addMode = delMode = false;
    showLCD("ADMIN OFF", "", 1500);
  }
}

// =====================================================
// BLYNK WRITE - theo dung virtual pin tren dashboard
// =====================================================

BLYNK_WRITE(V0)  { digitalWrite(LED1, param.asInt()); }          // led1
BLYNK_WRITE(V1)  { digitalWrite(LED2, param.asInt()); }          // led2
BLYNK_WRITE(V2)  { digitalWrite(LED3, param.asInt()); }          // led3
// V3 = led4 - danh rieng cho gas alert, khong dieu khien tu app

BLYNK_WRITE(V4) {                                                  // cua
  if (param.asInt() == 1) openDoor();
  else closeDoor();
}

BLYNK_WRITE(V5) {                                                  // quat
  fanManual = (param.asInt() == 1);
  digitalWrite(FAN_PIN, param.asInt());
}

BLYNK_WRITE(V9) {                                                  // add the
  if (!adminMode) return;
  addMode = param.asInt();
  if (addMode) { delMode = false; showLCD("QUET THE", "DE THEM", 1500); }
  adminTime = millis();
}

BLYNK_WRITE(V10) {                                                 // xoa the
  if (!adminMode) return;
  delMode = param.asInt();
  if (delMode) { addMode = false; showLCD("QUET THE", "DE XOA", 1500); }
  adminTime = millis();
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  lcd.init(); lcd.backlight();
  showLCD("KHOI DONG", "HE THONG", 1000);

  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED1, OUTPUT); pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT); pinMode(LED4, OUTPUT);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(LED1, LOW); digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW); digitalWrite(LED4, LOW);

  SPI.begin();
  rfid.PCD_Init();

  door.attach(SERVO_PIN);
  delay(500);
  door.write(0);

  dht.begin();
  uidList[uidCount++] = adminUID;

  bool wifiOK  = connectWiFi();
  bool blynkOK = false;
  if (wifiOK) blynkOK = connectBlynk();

  systemReady = (wifiOK && blynkOK);
  showLCD(systemReady ? "SYSTEM READY" : "CHUA KET NOI", "", 1500);

  timer.setInterval(1000L,  readAndSend);
  timer.setInterval(15000L, reconnectSystem);
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  if (WiFi.status() == WL_CONNECTED && Blynk.connected())
    Blynk.run();

  timer.run();
  checkRFID();
  autoCloseDoor();
  checkAdminTimeout();
}