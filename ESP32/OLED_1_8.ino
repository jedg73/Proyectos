#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// Pines ESP32 para el TFT
#define TFT_CS   5    // Chip Select
#define TFT_DC   16   // Data/Command
#define TFT_RST  17   // Reset

// SPI por hardware (VSPI: SCK=18, MISO=19, MOSI=23)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Parámetros de la pelota
int ballX = 30;
int ballY = 30;
int lastBallX = 30;
int lastBallY = 30;
const int ballR = 10;

int velX = 2;
int velY = 2;

// Colores para ir cambiando en cada rebote
uint16_t colors[] = {
  ST77XX_RED,
  ST77XX_GREEN,
  ST77XX_BLUE,
  ST77XX_YELLOW,
  ST77XX_CYAN,
  ST77XX_MAGENTA,
  ST77XX_WHITE
};
uint8_t colorIndex = 0;
uint16_t currentColor = ST77XX_RED;

void cambiarColor() {
  colorIndex = (colorIndex + 1) % (sizeof(colors) / sizeof(colors[0]));
  currentColor = colors[colorIndex];
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando TFT pelota...");

  SPI.begin(18, 19, 23, TFT_CS);  // SCK, MISO, MOSI, SS

  tft.initR(INITR_BLACKTAB);      // Usa el mismo modo que te funcionó antes
  tft.setRotation(1);             // Horizontal
  tft.fillScreen(ST77XX_BLACK);   // Fondo negro

  // Pelota inicial
  tft.fillCircle(ballX, ballY, ballR, currentColor);
}

void loop() {
  // 1) Borrar la pelota anterior (solo esa zona)
  tft.fillCircle(lastBallX, lastBallY, ballR, ST77XX_BLACK);

  // 2) Actualizar posición
  ballX += velX;
  ballY += velY;

  bool rebote = false;

  // 3) Colisión en X
  if (ballX - ballR <= 0) {
    ballX = ballR;
    velX = -velX;
    rebote = true;
  } else if (ballX + ballR >= tft.width() - 1) {
    ballX = tft.width() - 1 - ballR;
    velX = -velX;
    rebote = true;
  }

  // 4) Colisión en Y
  if (ballY - ballR <= 0) {
    ballY = ballR;
    velY = -velY;
    rebote = true;
  } else if (ballY + ballR >= tft.height() - 1) {
    ballY = tft.height() - 1 - ballR;
    velY = -velY;
    rebote = true;
  }

  // 5) Si rebotó, cambiamos de color
  if (rebote) {
    cambiarColor();
  }

  // 6) Dibujar la pelota nueva
  tft.fillCircle(ballX, ballY, ballR, currentColor);

  // 7) Guardar posición para borrarla luego
  lastBallX = ballX;
  lastBallY = ballY;

  // 8) Velocidad de animación
  delay(10);  // sube a 15–20 si quieres más lento
}
