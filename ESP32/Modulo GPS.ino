/*
   Este programa conecta un ESP32 con un módulo GPS NEO-6M (GY-GPS6MV2)
   y una pantalla OLED de 0.96" (SSD1306 por I2C). Muestra en la pantalla
   la posición (Lat/Lng), número de satélites, HDOP, velocidad y la hora,
   la cual se convierte automáticamente de UTC a hora local (UTC-5, Perú).
   Se puede cambiar entre tres pantallas distintas usando el botón BOOT
   (GPIO0) del propio ESP32. También se incluyen mensajes de diagnóstico
   cuando no llegan datos NMEA o el flujo se detiene.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>

// ---- Configuración de la pantalla OLED ----
#define OLED_W 128
#define OLED_H 64
#define OLED_ADDR 0x3C
#define I2C_SDA 21
#define I2C_SCL 22
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);

// ---- Configuración del GPS usando Serial2 ----
static const uint32_t GPS_BAUD = 9600;
static const int GPS_RX_PIN = 16; // RX2 del ESP32, aquí conectamos el TX del GPS
static const int GPS_TX_PIN = 17; // TX2 del ESP32, opcional si quieres mandar datos al GPS
TinyGPSPlus gps;

// ---- Botón para cambiar de pantalla (usamos el BOOT del ESP32) ----
#define BTN_PIN 0
volatile uint8_t page = 0;   // página actual en la pantalla
unsigned long lastBtnMs = 0;
const unsigned long DEBOUNCE_MS = 180;

// ---- Variables de diagnóstico ----
unsigned long lastChars = 0, lastChangeMs = 0;

// ---- Funciones auxiliares ----
void twoDigits(Adafruit_SSD1306 &d, int v) {
  if (v < 10) d.print('0');
  d.print(v);
}

void centered(const String& s, int y, uint8_t size = 1) {
  display.setTextSize(size);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  int x = (OLED_W - w) / 2;
  display.setCursor(x, y);
  display.println(s);
}

void splash(const __FlashStringHelper* l1, const __FlashStringHelper* l2) {
  display.clearDisplay();
  centered("ESP32 + GPS + OLED", 0);
  display.setTextSize(1);
  display.setCursor(0, 20); display.println(l1);
  display.setCursor(0, 32); display.println(l2);
  display.display();
}

// capturamos también una línea cruda NMEA para mostrarla en la pantalla de diagnóstico
static char nmeaLine[100];
static size_t nmeaIdx = 0;
void feedGPS() {
  while (Serial2.available() > 0) {
    char c = Serial2.read();
    gps.encode(c);
    if (c == '\n' || nmeaIdx >= sizeof(nmeaLine) - 1) {
      nmeaLine[nmeaIdx] = 0;
      nmeaIdx = 0;
    } else if (c != '\r') {
      nmeaLine[nmeaIdx++] = c;
    }
  }
}

// mensaje cuando no llega absolutamente nada del GPS
void drawNoNMEA() {
  display.clearDisplay();
  centered("NO NMEA", 0, 2);
  display.setTextSize(1);
  display.setCursor(0, 22); display.println(F("Revisa:"));
  display.println(F("- TX GPS -> GPIO16"));
  display.println(F("- Baud = 9600"));
  display.println(F("- GND comun"));
  display.display();
}

// mensaje cuando el flujo de datos se corta
void drawStoppedNMEA() {
  display.clearDisplay();
  centered("NMEA detenido", 0, 2);
  display.setTextSize(1);
  display.setCursor(0, 22);
  display.println(F("Chequea cable/baud"));
  display.display();
}

// convierte la hora UTC que da el GPS a hora local de Perú (UTC-5)
void printLocalTimeUTCMinus5() {
  if (!gps.date.isValid() || !gps.time.isValid()) {
    display.println(F("Fecha/Hora: --"));
    return;
  }
  int y = gps.date.year();
  int m = gps.date.month();
  int d = gps.date.day();
  int hh = gps.time.hour();
  int mm = gps.time.minute();
  int ss = gps.time.second();

  // le restamos 5 horas
  hh -= 5;
  if (hh < 0) {
    hh += 24;
    d -= 1;
    if (d <= 0) {
      m -= 1;
      if (m <= 0) { m = 12; y -= 1; }
      int daysInMonth;
      if (m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12) daysInMonth = 31;
      else if (m == 4 || m == 6 || m == 9 || m == 11) daysInMonth = 30;
      else {
        bool leap = ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
        daysInMonth = leap ? 29 : 28;
      }
      d = daysInMonth;
    }
  }

  display.print(F("Fec: "));
  twoDigits(display, d); display.print('/');
  twoDigits(display, m); display.print('/');
  display.println(y);

  display.print(F("Hora (PE): "));
  twoDigits(display, hh); display.print(':');
  twoDigits(display, mm); display.print(':');
  twoDigits(display, ss);
  display.println();
}

// primera pantalla: datos principales
void drawPage0_Main() {
  display.clearDisplay();
  display.setCursor(0, 0);

  if (gps.location.isValid()) {
    display.print(F("Lat: ")); display.println(gps.location.lat(), 6);
    display.print(F("Lng: ")); display.println(gps.location.lng(), 6);
  } else {
    centered("Sin FIX", 0, 2);
    display.setTextSize(1);
    display.setCursor(0, 22);
    display.println(F("Coloca el modulo a cielo"));
    display.println(F("abierto, antena arriba"));
  }

  display.print(F("Sats: "));
  if (gps.satellites.isValid()) display.println((int)gps.satellites.value()); else display.println(F("--"));

  display.print(F("HDOP: "));
  if (gps.hdop.isValid()) display.println(gps.hdop.hdop(), 1); else display.println(F("--"));

  display.print(F("Vel: "));
  if (gps.speed.isValid()) { display.print(gps.speed.kmph(), 1); display.println(F(" km/h")); }
  else display.println(F("--"));

  display.print(F("Age(ms): "));
  display.println(gps.location.age());

  display.display();
}

// segunda pantalla: lat/lng grandes y la hora local
void drawPage1_Time() {
  display.clearDisplay();
  display.setCursor(0, 0);

  if (gps.location.isValid()) {
    display.setTextSize(2);
    display.print(F("Lat "));
    display.setTextSize(1); display.println(gps.location.lat(), 6);
    display.setTextSize(2);
    display.print(F("Lng "));
    display.setTextSize(1); display.println(gps.location.lng(), 6);
  } else {
    centered("Sin FIX", 0, 2);
  }
  display.println();

  printLocalTimeUTCMinus5();

  display.display();
}

// tercera pantalla: muestra la última línea NMEA y datos crudos
void drawPage2_NMEA() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Ultima NMEA:"));
  display.println(nmeaLine);
  display.println();

  display.print(F("Chars: "));
  display.println(gps.charsProcessed());
  display.print(F("Sats: "));
  if (gps.satellites.isValid()) display.println((int)gps.satellites.value()); else display.println(F("--"));
  display.print(F("HDOP: "));
  if (gps.hdop.isValid()) display.println(gps.hdop.hdop(), 1); else display.println(F("--"));

  display.display();
}

// detecta pulsación del botón BOOT y cambia la pantalla
void handleButton() {
  int val = digitalRead(BTN_PIN);
  unsigned long now = millis();
  if (val == LOW && (now - lastBtnMs) > DEBOUNCE_MS) { // activo en bajo
    lastBtnMs = now;
    page = (page + 1) % 3; // rotamos entre 0,1,2
  }
}

// ---- Setup principal ----
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(BTN_PIN, INPUT_PULLUP); // usamos BOOT como botón normal

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("No se pudo iniciar la OLED"));
    while (true) { delay(10); }
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  splash(F("Listo!"), F("Pulsa BOOT para cambiar"));
  delay(1200);
}

// ---- Loop principal ----
void loop() {
  feedGPS();
  handleButton();

  // chequeamos flujo de NMEA
  unsigned long chars = gps.charsProcessed();
  if (chars == 0) { drawNoNMEA(); delay(250); return; }
  if (chars != lastChars) { lastChars = chars; lastChangeMs = millis(); }
  else if (millis() - lastChangeMs > 3000) { drawStoppedNMEA(); delay(250); return; }

  // dibuja la pantalla actual
  if (page == 0)       drawPage0_Main();
  else if (page == 1)  drawPage1_Time();
  else                 drawPage2_NMEA();

  delay(120); // refresco suave
}

