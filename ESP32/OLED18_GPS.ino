/*
  ESP32 + TFT 1.8" ST7735 (SPI) + GPS GY-GPS6MV2 (NEO-6M)
  -------------------------------------------------------
  - Pantalla en vertical
  - Muestra estado del GPS:
      * Sin conexión (nunca tuvo fix)
      * Buena señal (con fix y suficientes satélites)
      * Señal perdida (tuvo fix, pero dejó de recibir datos)
  - Cuando se pierde la señal:
      * Mantiene la última posición conocida
      * Muestra cuánto tiempo llevamos sin señal (cronómetro)
      * Muestra una hora UTC estimada a partir de la última hora GPS
      * Muestra una hora local estimada con GMT calculado a partir de la longitud
        (aproximado, solo a modo orientativo)

  Conexiones:

    TFT 1.8" ST7735
      VCC  ->  3V3
      GND  ->  GND
      LED  ->  3V3
      SCK  ->  GPIO18  (SCK VSPI)
      SDA  ->  GPIO23  (MOSI VSPI)
      CS   ->  GPIO5
      DC   ->  GPIO16
      RST  ->  GPIO17

    GPS GY-GPS6MV2 (NEO-6M)
      VCC  ->  3V3
      GND  ->  GND
      TX   ->  GPIO26  (RX2 del ESP32)
      RX   ->  GPIO27  (TX2 del ESP32, opcional)
*/

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <TinyGPSPlus.h>
#include <math.h>

// ---------- Pines del TFT ----------
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ---------- Pines del GPS ----------
#define GPS_RX_PIN 26   // RX2 del ESP32 (lee el TX del GPS)
#define GPS_TX_PIN 27   // TX2 del ESP32 (por si queremos enviar comandos al GPS)

TinyGPSPlus gps;

// ---------- Estados de la lógica ----------
bool algunaVezTuvoFix = false;   // Indica si alguna vez se obtuvo una posición válida
bool ahoraBuenaSenal  = false;   // Indica si en este momento hay buena señal
bool senalPerdida     = false;   // Indica que hubo fix pero se dejó de recibir datos

// Momento (en millis) de la última fix válida
unsigned long millisUltimaFix = 0;

// Cada cuánto refrescamos lo que se ve en pantalla
unsigned long ultimoRefrescoPantalla = 0;
const unsigned long intervaloRefrescoMs = 1000;  // 1 segundo

// Si pasa más de este tiempo sin datos nuevos, asumimos que se perdió la señal
const unsigned long tiempoParaPerderSenalMs = 5000;  // 5 segundos

// Offset horario aproximado respecto a UTC (en horas), calculado por la longitud
int offsetHoraGMT = 0;

// ---------- Estados de pantalla ----------
enum EstadoPantalla {
  PANTALLA_INICIAL = 0,
  PANTALLA_BUENA_SENAL,
  PANTALLA_SENAL_PERDIDA
};

EstadoPantalla estadoPantalla = PANTALLA_INICIAL;
EstadoPantalla ultimoEstadoPantalla = PANTALLA_INICIAL;

// ---------- Últimos datos GPS conocidos ----------
double ultimaLat = 0.0;
double ultimaLng = 0.0;
double ultimoHdop = 0.0;
double ultimaAltura = 0.0;      // Altitud sobre el nivel del mar en metros
double ultimaVelocidad = 0.0;   // Velocidad en km/h
uint32_t ultimosSats = 0;

uint8_t ultimaHora = 0;
uint8_t ultimoMinuto = 0;
uint8_t ultimoSegundo = 0;

uint8_t ultimoDia = 0;
uint8_t ultimoMes = 0;
uint16_t ultimoAnio = 0;

// ---------- Posiciones (Y) para las líneas de texto ----------
const int X_ETIQUETA = 5;    // Inicio del texto "Sat:", "Lat:", etc.
const int X_VALOR    = 60;   // Inicio del valor numérico

const int Y_SAT    = 42;
const int Y_HDOP   = Y_SAT   + 10;
const int Y_LAT    = Y_HDOP  + 10;
const int Y_LON    = Y_LAT   + 10;
const int Y_ALT    = Y_LON   + 10;
const int Y_VEL    = Y_ALT   + 10;
const int Y_FECHA  = Y_VEL   + 10;
const int Y_UTC    = Y_FECHA + 10;
const int Y_HLOCAL = Y_UTC   + 10;

const int Y_PERD_LASTFIX = 42;
const int Y_PERD_SAT     = Y_PERD_LASTFIX + 10;
const int Y_PERD_LAT     = Y_PERD_SAT     + 10;
const int Y_PERD_LON     = Y_PERD_LAT     + 10;
const int Y_PERD_ALT     = Y_PERD_LON     + 10;
const int Y_PERD_SIN     = Y_PERD_ALT     + 10;
const int Y_PERD_UTC     = Y_PERD_SIN     + 10;
const int Y_PERD_LOCAL   = Y_PERD_UTC    + 10;

// ================================================================
//  Funciones de ayuda para hora local y GMT
// ================================================================

// A partir de la longitud en grados, aproximamos el offset horario GMT.
// Esto es una aproximación muy simple: zona horaria ~ longitud / 15.
// No tiene en cuenta fronteras ni horario de verano, pero sirve de referencia.
int calcularOffsetGMTaprox(double lon) {
  double off = lon / 15.0;
  int i = (int)(off >= 0 ? off + 0.5 : off - 0.5);  // redondeo
  if (i < -12) i = -12;
  if (i >  14) i = 14;
  return i;
}

// A partir de una hora UTC (h, m, s) y un offset de horas, calcula la hora local
void calcularHoraLocalDesdeUTC(uint8_t utcH, uint8_t utcM, uint8_t utcS,
                               int offsetHoras,
                               uint8_t &locH, uint8_t &locM, uint8_t &locS) {
  long totalSeg = (long)utcH * 3600L + (long)utcM * 60L + (long)utcS +
                  (long)offsetHoras * 3600L;

  long unDia = 24L * 3600L;
  while (totalSeg < 0)      totalSeg += unDia;
  while (totalSeg >= unDia) totalSeg -= unDia;

  locH = totalSeg / 3600L;
  locM = (totalSeg / 60L) % 60L;
  locS = totalSeg % 60L;
}

// Imprime en una línea algo tipo: "GMT-05 12:34:56"
void imprimirGMTyHora(int x, int y, int offset, uint8_t h, uint8_t m, uint8_t s) {
  tft.setCursor(x, y);

  tft.print("GMT");
  if (offset >= 0) tft.print('+');
  else             tft.print('-');

  int absOff = offset >= 0 ? offset : -offset;
  if (absOff < 10) tft.print('0');
  tft.print(absOff);
  tft.print(' ');

  if (h < 10) tft.print('0');
  tft.print(h);
  tft.print(':');
  if (m < 10) tft.print('0');
  tft.print(m);
  tft.print(':');
  if (s < 10) tft.print('0');
  tft.print(s);
}

// ================================================================
//  Funciones de pantalla
// ================================================================

void dibujarMarcoPantalla() {
  tft.setRotation(0);          // Pantalla en vertical
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  tft.setCursor(5, 2);
  tft.print("GPS NEO-6M");

  tft.drawLine(0, 14, tft.width(), 14, ST77XX_WHITE);

  tft.setCursor(5, 17);
  tft.print("Estado:");
}

void limpiarTextoEstado() {
  tft.fillRect(5, 27, tft.width() - 10, 10, ST77XX_BLACK);
}

void mostrarTextoEstado(const char* texto, uint16_t colorTexto) {
  limpiarTextoEstado();
  tft.setCursor(5, 27);
  tft.setTextColor(colorTexto, ST77XX_BLACK);
  tft.print(texto);
}

void limpiarZonaDatos() {
  tft.fillRect(0, 40, tft.width(), tft.height() - 40, ST77XX_BLACK);
}

void limpiarZonaValor(int x, int y) {
  tft.fillRect(x, y, tft.width() - x - 2, 8, ST77XX_BLACK);
}

// ---------- PANTALLA: Sin conexión (nunca tuvo fix) ----------

void dibujarPantallaSinConexion() {
  mostrarTextoEstado("Sin conexion GPS", ST77XX_YELLOW);
  limpiarZonaDatos();

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(5, 42);
  tft.print("Lleve el modulo");
  tft.setCursor(5, 52);
  tft.print("a cielo abierto.");
}

// ---------- PANTALLA: Buena señal ----------

void dibujarPantallaBuenaSenal() {
  mostrarTextoEstado("Buena senal", ST77XX_GREEN);
  limpiarZonaDatos();

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  tft.setCursor(X_ETIQUETA, Y_SAT);
  tft.print("Sat:");

  tft.setCursor(X_ETIQUETA, Y_HDOP);
  tft.print("HDOP:");

  tft.setCursor(X_ETIQUETA, Y_LAT);
  tft.print("Lat:");

  tft.setCursor(X_ETIQUETA, Y_LON);
  tft.print("Lon:");

  tft.setCursor(X_ETIQUETA, Y_ALT);
  tft.print("Alt:");

  tft.setCursor(X_ETIQUETA, Y_VEL);
  tft.print("Vel:");

  tft.setCursor(X_ETIQUETA, Y_FECHA);
  tft.print("Fecha:");

  tft.setCursor(X_ETIQUETA, Y_UTC);
  tft.print("UTC:");

  tft.setCursor(X_ETIQUETA, Y_HLOCAL);
  tft.print("Local:");
}

void actualizarPantallaBuenaSenal() {
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  // Satélites
  limpiarZonaValor(X_VALOR, Y_SAT);
  tft.setCursor(X_VALOR, Y_SAT);
  tft.print(ultimosSats);

  // HDOP
  limpiarZonaValor(X_VALOR, Y_HDOP);
  tft.setCursor(X_VALOR, Y_HDOP);
  tft.print(ultimoHdop, 1);

  // Latitud
  limpiarZonaValor(X_VALOR, Y_LAT);
  tft.setCursor(X_VALOR, Y_LAT);
  tft.print(ultimaLat, 6);

  // Longitud
  limpiarZonaValor(X_VALOR, Y_LON);
  tft.setCursor(X_VALOR, Y_LON);
  tft.print(ultimaLng, 6);

  // Altitud sobre el nivel del mar, en metros
  limpiarZonaValor(X_VALOR, Y_ALT);
  tft.setCursor(X_VALOR, Y_ALT);
  tft.print(ultimaAltura, 1);
  tft.print(" m");

  // Velocidad en km/h
  limpiarZonaValor(X_VALOR, Y_VEL);
  tft.setCursor(X_VALOR, Y_VEL);
  tft.print(ultimaVelocidad, 1);
  tft.print(" km/h");

  // Fecha (del GPS, en UTC)
  limpiarZonaValor(X_VALOR, Y_FECHA);
  tft.setCursor(X_VALOR, Y_FECHA);
  if (ultimoDia < 10) tft.print('0');
  tft.print(ultimoDia);
  tft.print('/');
  if (ultimoMes < 10) tft.print('0');
  tft.print(ultimoMes);
  tft.print('/');
  tft.print(ultimoAnio);

  // Hora UTC
  limpiarZonaValor(X_VALOR, Y_UTC);
  tft.setCursor(X_VALOR, Y_UTC);
  if (ultimaHora < 10) tft.print('0');
  tft.print(ultimaHora);
  tft.print(':');
  if (ultimoMinuto < 10) tft.print('0');
  tft.print(ultimoMinuto);
  tft.print(':');
  if (ultimoSegundo < 10) tft.print('0');
  tft.print(ultimoSegundo);

  // Hora local aproximada (GMT según longitud)
  uint8_t hLoc, mLoc, sLoc;
  calcularHoraLocalDesdeUTC(ultimaHora, ultimoMinuto, ultimoSegundo,
                            offsetHoraGMT,
                            hLoc, mLoc, sLoc);

  limpiarZonaValor(X_VALOR, Y_HLOCAL);
  imprimirGMTyHora(X_VALOR, Y_HLOCAL, offsetHoraGMT, hLoc, mLoc, sLoc);
}

// ---------- PANTALLA: Señal perdida ----------

void dibujarPantallaSenalPerdida() {
  mostrarTextoEstado("Senal perdida", ST77XX_RED);
  limpiarZonaDatos();

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  tft.setCursor(X_ETIQUETA, Y_PERD_LASTFIX);
  tft.print("Ultima fix:");

  tft.setCursor(X_ETIQUETA, Y_PERD_SAT);
  tft.print("Sat:");

  tft.setCursor(X_ETIQUETA, Y_PERD_LAT);
  tft.print("Lat:");

  tft.setCursor(X_ETIQUETA, Y_PERD_LON);
  tft.print("Lon:");

  tft.setCursor(X_ETIQUETA, Y_PERD_ALT);
  tft.print("Alt:");

  tft.setCursor(X_ETIQUETA, Y_PERD_SIN);
  tft.print("Sin senal:");

  tft.setCursor(X_ETIQUETA, Y_PERD_UTC);
  tft.print("UTC est:");

  tft.setCursor(X_ETIQUETA, Y_PERD_LOCAL);
  tft.print("Local est:");
}

// Calcula una hora UTC aproximada sumando el tiempo transcurrido a la última hora conocida
void calcularUTCDesdeUltimaFix(uint8_t &h, uint8_t &m, uint8_t &s) {
  unsigned long segundosPasados = (millis() - millisUltimaFix) / 1000UL;

  unsigned long totalSegundos = (unsigned long)ultimaHora * 3600UL +
                                (unsigned long)ultimoMinuto * 60UL +
                                (unsigned long)ultimoSegundo +
                                segundosPasados;

  h = (totalSegundos / 3600UL) % 24;
  m = (totalSegundos / 60UL) % 60;
  s = totalSegundos % 60;
}

// Actualiza los datos cuando la señal se ha perdido
void actualizarPantallaSenalPerdida() {
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  // Últimos datos de posición y satélites
  limpiarZonaValor(X_VALOR, Y_PERD_SAT);
  tft.setCursor(X_VALOR, Y_PERD_SAT);
  tft.print(ultimosSats);

  limpiarZonaValor(X_VALOR, Y_PERD_LAT);
  tft.setCursor(X_VALOR, Y_PERD_LAT);
  tft.print(ultimaLat, 6);

  limpiarZonaValor(X_VALOR, Y_PERD_LON);
  tft.setCursor(X_VALOR, Y_PERD_LON);
  tft.print(ultimaLng, 6);

  limpiarZonaValor(X_VALOR, Y_PERD_ALT);
  tft.setCursor(X_VALOR, Y_PERD_ALT);
  tft.print(ultimaAltura, 1);
  tft.print(" m");

  // Cronómetro de tiempo sin señal
  unsigned long segundosSinSenal = (millis() - millisUltimaFix) / 1000UL;
  unsigned int minutosSin = segundosSinSenal / 60;
  unsigned int segundosSin = segundosSinSenal % 60;

  limpiarZonaValor(X_VALOR, Y_PERD_SIN);
  tft.setCursor(X_VALOR, Y_PERD_SIN);
  if (minutosSin < 10) tft.print('0');
  tft.print(minutosSin);
  tft.print(':');
  if (segundosSin < 10) tft.print('0');
  tft.print(segundosSin);

  // Hora UTC estimada a partir de la última fix
  uint8_t hUTC, mUTC, sUTC;
  calcularUTCDesdeUltimaFix(hUTC, mUTC, sUTC);

  limpiarZonaValor(X_VALOR, Y_PERD_UTC);
  tft.setCursor(X_VALOR, Y_PERD_UTC);
  if (hUTC < 10) tft.print('0');
  tft.print(hUTC);
  tft.print(':');
  if (mUTC < 10) tft.print('0');
  tft.print(mUTC);
  tft.print(':');
  if (sUTC < 10) tft.print('0');
  tft.print(sUTC);

  // Hora local estimada a partir de esa UTC y del GMT aproximado
  uint8_t hLoc, mLoc, sLoc;
  calcularHoraLocalDesdeUTC(hUTC, mUTC, sUTC, offsetHoraGMT, hLoc, mLoc, sLoc);

  limpiarZonaValor(X_VALOR, Y_PERD_LOCAL);
  imprimirGMTyHora(X_VALOR, Y_PERD_LOCAL, offsetHoraGMT, hLoc, mLoc, sLoc);
}

// ================================================================
//  SETUP
// ================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Arrancando ESP32 + TFT + GPS...");

  SPI.begin(18, 19, 23, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  Serial2.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  dibujarMarcoPantalla();
  dibujarPantallaSinConexion();
}

// ================================================================
//  LOOP
// ================================================================

void loop() {
  // Leer datos del GPS todo el tiempo
  while (Serial2.available() > 0) {
    char c = Serial2.read();
    gps.encode(c);
  }

  unsigned long ahora = millis();
  bool hayFixNuevaBuena = false;

  // Si los datos son válidos, los tomamos como nueva fix
  if (gps.location.isValid() &&
      gps.time.isValid() &&
      gps.date.isValid() &&
      gps.satellites.isValid() &&
      gps.satellites.value() >= 3) {

    ultimaLat       = gps.location.lat();
    ultimaLng       = gps.location.lng();
    ultimosSats     = gps.satellites.value();
    ultimoHdop      = gps.hdop.isValid()     ? gps.hdop.hdop()       : 0.0;
    ultimaAltura    = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
    ultimaVelocidad = gps.speed.isValid()    ? gps.speed.kmph()      : 0.0;

    ultimaHora    = gps.time.hour();
    ultimoMinuto  = gps.time.minute();
    ultimoSegundo = gps.time.second();

    ultimoDia   = gps.date.day();
    ultimoMes   = gps.date.month();
    ultimoAnio  = gps.date.year();

    millisUltimaFix   = ahora;
    algunaVezTuvoFix  = true;
    hayFixNuevaBuena  = true;

    // Calcular offset GMT aproximado según la nueva longitud
    offsetHoraGMT = calcularOffsetGMTaprox(ultimaLng);
  }

  // Actualizar estado lógico
  if (hayFixNuevaBuena) {
    ahoraBuenaSenal = true;
    senalPerdida    = false;
  } else {
    if (algunaVezTuvoFix && (ahora - millisUltimaFix > tiempoParaPerderSenalMs)) {
      ahoraBuenaSenal = false;
      senalPerdida    = true;
    }
  }

  // Decidir qué pantalla usar
  if (!algunaVezTuvoFix) {
    estadoPantalla = PANTALLA_INICIAL;
  } else if (ahoraBuenaSenal) {
    estadoPantalla = PANTALLA_BUENA_SENAL;
  } else if (senalPerdida) {
    estadoPantalla = PANTALLA_SENAL_PERDIDA;
  }

  // Refrescar pantalla cada cierto tiempo
  if (ahora - ultimoRefrescoPantalla >= intervaloRefrescoMs) {
    ultimoRefrescoPantalla = ahora;

    bool cambioDePantalla = (estadoPantalla != ultimoEstadoPantalla);
    ultimoEstadoPantalla = estadoPantalla;

    switch (estadoPantalla) {
      case PANTALLA_INICIAL:
        if (cambioDePantalla) {
          dibujarPantallaSinConexion();
        }
        break;

      case PANTALLA_BUENA_SENAL:
        if (cambioDePantalla) {
          dibujarPantallaBuenaSenal();
        }
        actualizarPantallaBuenaSenal();
        break;

      case PANTALLA_SENAL_PERDIDA:
        if (cambioDePantalla) {
          dibujarPantallaSenalPerdida();
        }
        actualizarPantallaSenalPerdida();
        break;
    }
  }
}
