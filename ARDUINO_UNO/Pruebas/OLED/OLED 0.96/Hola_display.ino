//Joaquin Eduardo Dávila García


#include <U8g2lib.h>
#include <Wire.h>

// SW I2C en D3(SCL), D2(SDA), sin RST
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 3, 2, U8X8_PIN_NONE);

void setup() {
  u8g2.setI2CAddress(0x3C*2);   
  u8g2.begin();                 
}

void loop() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0,12,"Hola OLED");
  u8g2.drawFrame(0,0,128,64);
  u8g2.sendBuffer();
  delay(500);
}
