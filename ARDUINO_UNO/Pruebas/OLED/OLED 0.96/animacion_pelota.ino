//JEDG
//una pelota animada que rebota en las paredes del OLED

#include <U8g2lib.h>

// OLED SSD1306 128x64 por I2C (HW). I2C en Arduino UNO: SDA=A4, SCL=A5.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

const uint8_t W = 128;      // ancho
const uint8_t H = 64;       // alto
const uint8_t r = 6;        // radio de la pelota

int16_t x = 10, y = 10;     // posición actual
int16_t px = 10, py = 10;   // posición previa
int8_t  vx = 3,  vy = 2;    // velocidad

void setup() {
  u8g2.setI2CAddress(0x3C * 2);  // dirección I2C (0x3C por defecto; usar 0x3D*2 si aplica)
  u8g2.setBusClock(400000);      // I2C a 400 kHz
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

void loop() {
  // borrar pelota previa
  u8g2.setDrawColor(0);
  u8g2.drawDisc(px, py, r);

  // actualizar posición y rebotes
  x += vx; 
  y += vy;
  if (x <= r || x >= (W - 1 - r)) vx = -vx;
  if (y <= r || y >= (H - 1 - r)) vy = -vy;

  // dibujar pelota nueva
  u8g2.setDrawColor(1);
  u8g2.drawDisc(x, y, r);

  // enviar al OLED
  u8g2.sendBuffer();

  // guardar posición para el siguiente frame
  px = x; 
  py = y;
}
