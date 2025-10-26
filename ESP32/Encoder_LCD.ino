/*
ESP32 + Encoder rotatorio (CLK/DT/SW) + OLED 0.96" SSD1306 (I2C)
Muestra en pantalla el ángulo (0–360°) calculado desde un encoder incremental.
Decodificación en cuadratura 4× por interrupciones; botón SW para poner a 0°.
Ajusta ENCODER_PPR al valor de tu encoder (CPR = 4×PPR).

Conexiones rápidas:
- OLED (SSD1306 I2C, 0x3C): SDA→GPIO21, SCL→GPIO22, VCC→3V3, GND→GND
- Encoder: CLK→GPIO25, DT→GPIO26, SW→GPIO27 (opcional), GND→GND
  * Usar INPUT_PULLUP. Si el sentido se invierte, intercambia CLK y DT.
*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- CONFIGURACIÓN ----------
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Encoder (CLK/DT y botón opcional)
#define ENC_CLK_PIN 25
#define ENC_DT_PIN  26
#define ENC_SW_PIN  27     // Opcional (si no lo usas, igual compila)

// Pulsos por revolución por canal (ajusta a tu encoder)
#define ENCODER_PPR 20     // KY-040 ~20; ópticos típicos 100–600
const int32_t ENCODER_CPR = ENCODER_PPR * 4;  // cuadratura 4x

// Refresco de pantalla (ms)
#define UI_INTERVAL_MS 100

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- VARIABLES GLOBALES ----------
volatile int32_t encoder_count = 0;   // cuenta (−∞..+∞) en 1/4 de paso
volatile uint8_t prevCD = 0;          // estado previo CLK/DT (2 bits)
uint32_t lastUiMs = 0;

// Tabla de transición en cuadratura (4x)
static const int8_t QUAD_DIR[16] = {
  0, -1, +1,  0,
 +1,  0,  0, -1,
 -1,  0,  0, +1,
  0, +1, -1,  0
};

inline uint8_t readCLKDT() {
  uint8_t clk = (uint8_t)digitalRead(ENC_CLK_PIN);
  uint8_t dt  = (uint8_t)digitalRead(ENC_DT_PIN);
  return (clk << 1) | dt;  // bit1=CLK, bit0=DT
}

void IRAM_ATTR isrEncoder() {
  uint8_t curr = readCLKDT();
  uint8_t idx = (prevCD << 2) | curr;
  int8_t step = QUAD_DIR[idx];
  if (step != 0) encoder_count += step;  // +1 o -1 por transición válida
  prevCD = curr;
}

// Normaliza a [0, CPR)
int32_t wrapCount(int32_t c) {
  int32_t m = c % ENCODER_CPR;
  if (m < 0) m += ENCODER_CPR;
  return m;
}

float countToDegrees(int32_t c) {
  int32_t w = wrapCount(c);
  return (360.0f * (float)w) / (float)ENCODER_CPR;
}

void drawAngle(float deg) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Encoder Angle"));

  display.setTextSize(2);
  display.setCursor(0, 20);
  char buf[24];
  snprintf(buf, sizeof(buf), " %6.1f deg", deg);
  display.println(buf);

  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print(F("PPR="));
  display.print(ENCODER_PPR);
  display.print(F("  CPR="));
  display.print(ENCODER_CPR);

  display.display();
}

void setup() {
  // I2C + OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Inicializando..."));
  display.display();

  // Encoder pins
  pinMode(ENC_CLK_PIN, INPUT_PULLUP);
  pinMode(ENC_DT_PIN,  INPUT_PULLUP);
  pinMode(ENC_SW_PIN,  INPUT_PULLUP); // opcional

  // Estado inicial + interrupciones
  prevCD = readCLKDT();
  attachInterrupt(digitalPinToInterrupt(ENC_CLK_PIN), isrEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT_PIN),  isrEncoder, CHANGE);

  delay(300);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Listo"));
  display.display();
}

void loop() {
  // Reset a cero con el botón (opcional, activo en LOW)
  if (digitalRead(ENC_SW_PIN) == LOW) {
    delay(10);
    if (digitalRead(ENC_SW_PIN) == LOW) {
      noInterrupts();
      encoder_count = 0;
      interrupts();
      while (digitalRead(ENC_SW_PIN) == LOW) { delay(5); } // espera soltar
    }
  }

  // Actualiza pantalla
  uint32_t now = millis();
  if (now - lastUiMs >= UI_INTERVAL_MS) {
    lastUiMs = now;

    int32_t cnt;
    noInterrupts();
    cnt = encoder_count;
    interrupts();

    float deg = countToDegrees(cnt);
    drawAngle(deg);
  }
}
