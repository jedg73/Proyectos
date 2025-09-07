//JEDG
// Bad Apple!! en OLED 128x64 I²C (SSD1306) con streaming por Serial y ACK.
// Esta .ino trae TODO: pasos, comandos y los dos scripts (abajo en comentarios).
//
// ╭──────────────────────────────────────────────────────────────────────────╮
// │ PASOS (Windows + PowerShell)                                            │
// ╰──────────────────────────────────────────────────────────────────────────╯
//
// Paso 0 — Requisitos rápidos
// • Arduino IDE con librerías: “Adafruit GFX Library” y “Adafruit SSD1306”.
// • Python 3 instalado (y pip).
// • Cableado UNO: VCC→5V, GND→GND, SDA→A4, SCL→A5.
// • Video fuente (ejemplo): https://ia601608.us.archive.org/17/items/bad-apple-resources/bad_apple.mp4
//
// Paso 1 — Instalar FFmpeg (sencillo con winget)
//   PowerShell:
//     winget install ffmpeg
//     (cerrar y abrir PowerShell)
//     ffmpeg -version
//
// Paso 2 — Carpeta de trabajo (usa tu propia ruta)
//   PowerShell (cambia C:\tu\ruta por donde tengas el video):
//     cd "C:\tu\ruta"
//     mkdir badapple; cd badapple
//     mkdir frames
//
// Paso 3 — Convertir el video a frames 1-bit 128×64 @ 20 fps
//   PowerShell (ajusta el nombre/ruta del video):
//     ffmpeg -i "C:\tu\ruta\bad_apple.mp4" -vf "fps=20,scale=128:64:flags=area" -pix_fmt monob ".\frames\out_%05d.pbm"
//
// Paso 4 — Crear el BIN con los frames (formato nativo SSD1306)
//   4.1) Copiar/pegar en PowerShell para crear el script make_badapple_bin.py:
//     @'
//     (contenido del script está más abajo; copia TODO desde “#JEDG - make_badapple_bin.py”
//      hasta la línea “print('Listo: BADAPPLE.BIN')”)
//     '@ | Set-Content -Encoding ASCII .\make_badapple_bin.py
//
//   4.2) Ejecutar el script para generar BADAPPLE.BIN:
//     python .\make_badapple_bin.py
//
// Paso 5 — Subir ESTE sketch .ino al Arduino y cerrar el Monitor Serie
// • Configura más abajo BAUD/I2C_ADDR si hace falta. Sube y cierra el Monitor Serie.
//
// Paso 6 — Enviar el BIN por Serial con ACK (flujo controlado)
//   6.1) Copiar/pegar en PowerShell para crear el script stream_badapple_ack.py:
//     @'
//     (contenido del script está más abajo; copia TODO desde “# JEDG - stream_badapple_ack.py”
//      hasta el final)
//     '@ | Set-Content -Encoding ASCII .\stream_badapple_ack.py
//
//   6.2) Instalar pyserial (si no lo tienes):
//     python -m pip install --user pyserial
//
//   6.3) Enviar (ajusta el COM del Arduino y deja el mismo BAUD que en el sketch):
//     python .\stream_badapple_ack.py COMx .\BADAPPLE.BIN 1000000
//     (si no arranca en ~2 s, pulsa RESET en el UNO)
//
// Paso 7 — Detalles útiles
// • Si tu OLED responde en 0x3D, cambia I2C_ADDR a 0x3D y vuelve a subir.
// • Si la imagen sale “negativa”, pon INVERT_BITS = 1 y recompila.
// • Para encontrar el puerto:   python -m serial.tools.list_ports -v
// • Si el COM aparece “ocupado”: cierra el Monitor Serie y cualquier app que lo use.
//
// ────────────────────────────────────────────────────────────────────────────
//  SKETCH (SSD1306 + ACK por frame)
// ────────────────────────────────────────────────────────────────────────────

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define W 128
#define H 64

// Config editable
#define BAUD        1000000   // Debe coincidir con el script de PC
#define I2C_ADDR    0x3C      // 0x3C típico; usar 0x3D si tu OLED responde en 0x3D
#define ROTATION    0         // 0..3
#define INVERT_BITS 0         // 0 normal, 1 invierte blanco/negro

Adafruit_SSD1306 display(W, H, &Wire, -1);

// lee exactamente n bytes desde Serial (bloqueante con el timeout de Serial)
bool readExact(uint8_t* dst, size_t n) {
  size_t got = 0;
  while (got < n) {
    int c = Serial.readBytes(dst + got, n - got);
    if (c <= 0) return false;
    got += c;
  }
  return true;
}

void setup() {
  // I²C (UNO: SDA=A4, SCL=A5)
  Wire.begin();
  Wire.setClock(400000);                  // 400 kHz para más throughput

  // OLED
  display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR);
  display.clearDisplay();
  display.setRotation(ROTATION);
  display.display();

  // Serial para streaming
  Serial.begin(BAUD);
  Serial.setTimeout(1000);
  Serial.println(F("RDY"));              // handshake para el script
}

void loop() {
  static bool synced = false;
  uint8_t *buf = display.getBuffer();    // framebuffer interno (1024 bytes)

  // Cabecera BIN: "BAF1"(4) + w(2) + h(2) + fps(2) + frames(4) = 14 bytes
  if (!synced) {
    if (Serial.available() < 14) return;

    char magic[4];
    if (!readExact((uint8_t*)magic, 4)) return;
    if (strncmp(magic, "BAF1", 4) != 0)  return;

    uint16_t w, h, fps;
    uint32_t frames;
    if (!readExact((uint8_t*)&w, 2))    return;
    if (!readExact((uint8_t*)&h, 2))    return;
    if (!readExact((uint8_t*)&fps, 2))  return;
    if (!readExact((uint8_t*)&frames, 4)) return;

    if (w != W || h != H) return;       // debe ser 128x64
    synced = true;                      // listo para recibir frames
  }

  // Frame: 1024 bytes directo al buffer del display
  if (!readExact(buf, (W * H) / 8)) { synced = false; return; }

  // invertir bits (opcional) si la imagen sale en negativo
  #if INVERT_BITS
    for (uint16_t i = 0; i < (W * H) / 8; i++) buf[i] = ~buf[i];
  #endif

  display.display();                     // envía el framebuffer al OLED
  Serial.write('K');                     // ACK por frame (el script espera este byte)
}

/*
───────────────────────────────────────────────────────────────────────────────
PASO 4 — SCRIPT: make_badapple_bin.py
───────────────────────────────────────────────────────────────────────────────
Copia todo este bloque (incluyendo la primera y última línea) y pégalo en
PowerShell tal cual para crear el archivo .\make_badapple_bin.py en tu carpeta
de trabajo. Luego ejecútalo con:  python .\make_badapple_bin.py

@'
#JEDG - make_badapple_bin.py
import glob, struct, os
W,H,FPS = 128,64,20

def pbm_to_ssd1306_bytes(path):
    with open(path,'rb') as f:
        assert f.readline().strip()==b'P4'   # PBM binario
        dims=f.readline().strip(); data=f.read()
    w,h=map(int,dims.split()); assert (w,h)==(W,H)
    bpr=(W+7)//8
    rows=[data[i*bpr:(i+1)*bpr] for i in range(H)]
    out=bytearray(W*H//8); pos=0
    # SSD1306: 8 filas por "página", cada byte = columna x, bits verticales (LSB=arriba)
    for page in range(0,H,8):      # 0..56
        for x in range(W):         # 0..127
            b=0
            for bit in range(8):   # 0..7
                y=page+bit
                byte_in_row=rows[y][x//8]
                bit_in_byte = 7-(x%8)      # PBM es MSB primero en horizontal
                pixel=(byte_in_row>>bit_in_byte)&1
                b |= (pixel<<bit)          # SSD1306: bit0 = fila superior del byte
            out[pos]=b; pos+=1
    return out

frames=sorted(glob.glob(os.path.join('frames','out_*.pbm')))
if not frames: raise SystemExit("No hay frames en .\\frames")
with open('BADAPPLE.BIN','wb') as out:
    out.write(b'BAF1')                          # cabecera
    out.write(struct.pack('<HHH',W,H,FPS))      # width, height, fps
    out.write(struct.pack('<I',len(frames)))    # cantidad de frames
    for i,fp in enumerate(frames,1):
        out.write(pbm_to_ssd1306_bytes(fp))
        if i%100==0: print('Frames:',i)
print('Listo: BADAPPLE.BIN')
'@
*/

/*
───────────────────────────────────────────────────────────────────────────────
PASO 6 — SCRIPT: stream_badapple_ack.py
───────────────────────────────────────────────────────────────────────────────
Copia todo este bloque (incluyendo la primera y última línea) y pégalo en
PowerShell tal cual para crear .\stream_badapple_ack.py. Luego:
  python .\stream_badapple_ack.py COMx .\BADAPPLE.BIN 1000000
(Usa el mismo BAUD que en el sketch y el COM que veas en el IDE.)

@'
# JEDG - stream_badapple_ack.py
import sys, time, serial, struct

def main(port, bin_path, baud=1000000, timeout=2.0):
    ser = serial.Serial(port, baudrate=baud, timeout=timeout)

    # esperar "RDY" del Arduino
    line = ser.readline().decode(errors='ignore').strip()
    while line != 'RDY':
        line = ser.readline().decode(errors='ignore').strip()

    with open(bin_path,'rb') as f:
        header = f.read(14)
        if not header.startswith(b'BAF1'): raise SystemExit("BIN invalido")
        w,h,fps,frames = struct.unpack('<4xHHHI', header)
        if (w,h)!=(128,64): raise SystemExit("Tamano distinto a 128x64")

        frame_bytes = 1024
        ser.write(header); ser.flush()   # enviar cabecera una vez

        while True:
            data = f.read(frame_bytes)
            if len(data) < frame_bytes:
                f.seek(14)               # loop al inicio de frames
                data = f.read(frame_bytes)

            ser.write(data); ser.flush() # enviar 1 frame

            # esperar ACK 'K' del Arduino antes del siguiente frame
            ack = ser.read(1)
            if not ack or ack != b'K':
                time.sleep(0.02)         # pequeño respiro si no llega ACK

if __name__ == "__main__":
    if len(sys.argv)<3:
        print("uso: python stream_badapple_ack.py COMx BADAPPLE.BIN [baud]")
        sys.exit(0)
    port=sys.argv[1]; path=sys.argv[2]
    baud=int(sys.argv[3]) if len(sys.argv)>=4 else 1000000
    main(port, path, baud)
'@
*/
