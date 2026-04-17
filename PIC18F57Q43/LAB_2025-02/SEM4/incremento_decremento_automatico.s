    PROCESSOR 18F57Q43
    #include "cabecera.inc"

;----------------------------------------------------------
; VARIABLES EN RAM (BANCO 5)
; Cada dirección almacena 1 byte = 8 bits
;----------------------------------------------------------
X            EQU 0500H      ; contador interno del retardo
Y            EQU 0501H      ; contador medio del retardo
Z            EQU 0502H      ; contador externo del retardo
valor_actual EQU 0503H      ; valor que se mostrará de 0 a 9

    PSECT upcino, class=CODE, reloc=2, abs

upcino:
    ORG 000000H
    bra configuro            ; al reset salta a la configuración inicial


;----------------------------------------------------------
; TABLA DE PATRONES PARA DISPLAY DE 7 SEGMENTOS
; Dirección base: 001000H
; Cada posición corresponde a un número decimal
;
; índice   patrón   número
;   0      3FH        0
;   1      06H        1
;   2      5BH        2
;   3      4FH        3
;   4      66H        4
;   5      6DH        5
;   6      7DH        6
;   7      07H        7
;   8      7FH        8
;   9      67H        9
;----------------------------------------------------------
    ORG 001000H

tabla_hex:
    DB 3FH, 06H, 5BH, 4FH, 66H, 6DH, 7DH, 07H
    DB 7FH, 67H


;----------------------------------------------------------
; CÓDIGO PRINCIPAL DESDE 000100H
;----------------------------------------------------------
    ORG 000100H

configuro:
    ;------------------------------------------------------
    ; CONFIGURACIÓN DEL OSCILADOR INTERNO
    ; OSCCON1 = 60H  -> selecciona HFINTOSC
    ; OSCFRQ  = 02H  -> frecuencia de 4 MHz
    ; OSCEN   = 40H  -> habilita oscilador interno
    ;------------------------------------------------------
    movlb 0H
    movlw 60H
    movwf OSCCON1,1
    movlw 02H
    movwf OSCFRQ,1          ; 4 MHz
    movlw 40H
    movwf OSCEN,1

    ;------------------------------------------------------
    ; CONFIGURACIÓN DE PUERTOS
    ; Se usan 7 líneas para cada display:
    ; PORTA -> RA0..RA6
    ; PORTF -> RF0..RF6
    ;
    ; 80H = 1000 0000b
    ; bit7 = 1 -> entrada
    ; bits6..0 = 0 -> salidas
    ;------------------------------------------------------
    movlb 4H

    movlw 80H
    movwf TRISA,1          ; RA7 entrada, RA6..RA0 salidas
    movwf ANSELA,1         ; RA7 analógico, RA6..RA0 digitales
    clrf  LATA,1           ; limpia salida del display en PORTA

    movlw 80H
    movwf TRISF,1          ; RF7 entrada, RF6..RF0 salidas
    movwf ANSELF,1         ; RF7 analógico, RF6..RF0 digitales
    clrf  LATF,1           ; limpia salida del display en PORTF

    ;------------------------------------------------------
    ; CONFIGURACIÓN DEL TBLPTR
    ; TBLPTR apunta a memoria de programa
    ;
    ; TBLPTRU = 00
    ; TBLPTRH = 10
    ; TBLPTRL = 00
    ;
    ; Entonces:
    ; TBLPTR = 001000H
    ; que es justo donde inicia la tabla_hex
    ;------------------------------------------------------
    clrf  TBLPTRU,1        ; parte alta del puntero = 00
    movlw 10H
    movwf TBLPTRH,1        ; parte media del puntero = 10
    clrf  TBLPTRL,1        ; parte baja del puntero = 00
                           ; TBLPTR = 001000H

    ;------------------------------------------------------
    ; INICIALIZA EL CONTADOR EN 0
    ;------------------------------------------------------
    movlb 5H
    clrf  valor_actual,1


;----------------------------------------------------------
; LAZO PRINCIPAL
; 1. Muestra valor_actual en PORTA
; 2. Muestra 9 - valor_actual en PORTF
; 3. Espera aproximadamente 500 ms
; 4. Incrementa valor_actual
; 5. Si llega a 10, reinicia a 0
;----------------------------------------------------------
principal:
    movlb 4H
    call mostrar_displayA   ; PORTA muestra 0,1,2,...,9
    call mostrar_displayF   ; PORTF muestra 9,8,7,...,0
    call retardo_500ms      ; espera aprox. medio segundo

    movlb 5H
    incf  valor_actual,1,1  ; valor_actual = valor_actual + 1

    movf  valor_actual,0,1  ; W = valor_actual
    xorlw 0AH               ; compara con 10 decimal
    bz    reiniciar         ; si valor_actual = 10, reinicia
    bra   principal         ; si no, sigue contando

reiniciar:
    clrf  valor_actual,1    ; vuelve a 0
    bra   principal


;----------------------------------------------------------
; SUBRUTINA: mostrar_displayA
; Muestra directamente valor_actual en el display de PORTA
;
; Ejemplo:
; si valor_actual = 3
; TBLPTRL = 03H
; TBLPTR = 001003H
; TBLRD* lee el patrón 4FH (número 3)
; y lo manda a LATA
;----------------------------------------------------------
mostrar_displayA:
    movlb 5H
    movf  valor_actual,0,1  ; W = valor_actual

    movlb 4H
    movwf TBLPTRL,1         ; índice dentro de la tabla
    TBLRD*                  ; lee el byte apuntado por TBLPTR -> TABLAT
    movf  TABLAT,0,1        ; W = patrón del número
    movwf LATA,1            ; envía el patrón al display en PORTA
    return


;----------------------------------------------------------
; SUBRUTINA: mostrar_displayF
; Muestra el conteo inverso en el display de PORTF
;
; Aquí no se usa valor_actual directamente:
; primero se calcula:
; W = 9 - valor_actual
;
; Ejemplo:
; si valor_actual = 2
; W = 9 - 2 = 7
; entonces se busca en la tabla el patrón del 7
; y se muestra en PORTF
;----------------------------------------------------------
mostrar_displayF:
    movlb 5H
    movf  valor_actual,0,1  ; W = valor_actual
    sublw 09H               ; W = 9 - valor_actual

    movlb 4H
    movwf TBLPTRL,1         ; índice inverso dentro de la tabla
    TBLRD*                  ; lee el patrón desde memoria de programa
    movf  TABLAT,0,1        ; W = patrón leído
    movwf LATF,1            ; envía el patrón al display en PORTF
    return


;----------------------------------------------------------
; SUBRUTINA: retardo_500ms
;
; Rutina de retardo por lazos anidados
; X y Y generan el retardo base
; Z controla cuántas veces se repite ese retardo base
;
; Valores usados:
; X = 56 = 38H
; Y = 6  = 06H
; Z = 250 = FAH
;
; Con el método aproximado usado en clase:
; T ≈ X * Y * Z * 6 us
; T ≈ 56 * 6 * 250 * 6 us
; T ≈ 504000 us
; T ≈ 504 ms
;
; Es decir, un retardo aproximado de medio segundo
;----------------------------------------------------------
retardo_500ms:
    movlb 5H

    movlw 0FAH             ; Z = 250 repeticiones del retardo base
    movwf Z,1

ZZ:
    movlw 06H              ; Y = 6
    movwf Y,1

YY:
    movlw 38H              ; X = 56
    movwf X,1

XX:
    decfsz X,1,1           ; decrementa X hasta 0
    bra XX

    decfsz Y,1,1           ; cuando X llega a 0, decrementa Y
    bra YY

    decfsz Z,1,1           ; cuando Y llega a 0, decrementa Z
    bra ZZ

    movlb 4H
    return

    end upcino
