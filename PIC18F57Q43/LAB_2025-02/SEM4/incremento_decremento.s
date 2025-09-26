    PROCESSOR 18F57Q43               
    #include "cabecera.inc"         

    PSECT upcino, class=CODE, reloc=2, abs  

upcino:
    ORG 000000H                       
    bra configuro          ; Salto a configuración inicial

    ORG 001000H                        ; Dirección de tablas 

tabla_hex:                              ; Tabla HEX 0..F 
    DB 3FH, 06H, 5BH, 4FH, 66H, 6DH, 7DH, 07H   ; 0..7
    DB 7FH, 67H, 77H, 7CH, 39H, 5EH, 79H, 71H   ; 8..F

tabla_gel:                              ; Tabla G/E/L 
    DB 3DH, 79H, 38H                    ; 'G', 'E', 'L'

    ORG 000100H                        

valor_actual  EQU 0500H                ; Variable 0x00..0x0F (banco 5)
hubo_cambio   EQU 0501H                ; Flag: 0 = no cambió, 1 = sí cambió (banco 5)


configuro:
    movlb 0H                           ; Selecciona banco 0 
    movlw 60H                          ; HFINTOSC como fuente
    movwf OSCCON1, 1                   ; Aplica fuente de reloj al sistema
    movlw 02H                          ; Frecuencia interna 4 MHz
    movwf OSCFRQ, 1                    ; Ajusta frecuencia
    movlw 40H                          ; Habilita oscilador interno
    movwf OSCEN, 1                     ; Enciende el oscilador

    movlb 4H                           ; Banco con puertos y ANSEL

    movlw 80H                     
    movwf TRISD, 1                     ; RD7 entrada, RD6..RD0 salidas
    movwf ANSELD, 1                    ; RD0..RD6 digitales 
    clrf  LATD, 1                      ; Apaga display1 inicialmente

    movlw 80H                          ; 1000 0000b
    movwf TRISF, 1                     ; RF7 entrada, RF6..RF0 salidas
    movwf ANSELF, 1                    ; RF0..RF6 digitales 
    clrf  LATF, 1                      ; Apaga display2 inicialmente

    movlw 00H                         
    movwf ANSELE, 1                    ; PORTE digital (E0/E1 botones)
    bsf   TRISE, 0, 1                  ; RE0 como entrada (botón incremento)
    bsf   TRISE, 1, 1                  ; RE1 como entrada (botón decremento)
    bsf   WPUE,  0, 1                  ; Pull-up débil en RE0
    bsf   WPUE,  1, 1                  ; Pull-up débil en RE1

    clrf  TBLPTRU, 1                   ; TBLPTRU = 00H (página alta)
    movlw 10H                          ; 0010 0000b -> 0x10
    movwf TBLPTRH, 1                   ; TBLPTRH = 10H => base 001000H
    clrf  TBLPTRL, 1                   ; TBLPTRL = 00H (puntero lista para tabla)

    movlb 5H                           ; Banco 5 (variables en 0x500+)
    clrf  valor_actual, 1              ; valor_actual = 0
    clrf  hubo_cambio, 1               ; hubo_cambio = 0
    movlb 4H                           ; Banco SFR para manejar puertos
    call mostrar_display1              ; Imprime 0 en display1
    call mostrar_display2              ; Imprime 'L' en display2 (0<5)


principal:
    movlb 4H                           ; Asegura banco SFR antes de leer PORTE

    btfsc PORTE, 0, 1                  ; ¿RE0 = 1? (no presionado por pull-up)
    bra   revisar_decremento           ; Si no se presiona RE0, ir a revisar RE1
    call  incrementar_valor            ; Si RE0 = 0 (presionado), intenta sumar

    movlb 5H                           ; Cambia a banco de la flag
    movf  hubo_cambio, 0, 1            ; W = hubo_cambio
    bz    revisar_decremento           ; Si no cambió (en F), no esperamos
    movlb 4H                           ; Volver a banco SFR para leer PORTE
esperar_liberar_re0:
    btfss PORTE, 0, 1                  ; Espera a que RE0 vuelva a 1 (soltado)
    bra   esperar_liberar_re0          ; Mientras esté en 0, sigue esperando

revisar_decremento:
    movlb 4H                           ; Banco SFR para leer PORTE

    btfsc PORTE, 1, 1                  ; ¿RE1 = 1? (no presionado)
    bra   principal                    ; Ningún botón, vuelve a inicio
    call  decrementar_valor            ; Si RE1 = 0 (presionado), intenta restar

    movlb 5H                           ; Banco de flag
    movf  hubo_cambio, 0, 1            ; W = hubo_cambio
    bz    principal                    ; Si no cambió (en 0), no esperamos
    movlb 4H                           ; Banco SFR para leer PORTE
esperar_liberar_re1:
    btfss PORTE, 1, 1                  ; Espera a que RE1 vuelva a 1 (soltado)
    bra   esperar_liberar_re1          ; Mientras esté en 0, sigue esperando
    bra   principal                    ; Vuelve a leer botones


incrementar_valor:
    movlb 5H                           ; Asegura banco de variables
    movf  valor_actual, 0, 1           ; W = valor_actual
    xorlw 0FH                          ; Compara con 0x0F (F)
    bz    no_incrementa                ; Si ya es F, no incrementa
    incf  valor_actual, 1, 1           ; valor_actual = valor_actual + 1
    movlw 01H                          ; W = 1
    movwf hubo_cambio, 1               ; hubo_cambio = 1 (sí cambió)
    movlb 4H                           ; Banco SFR para imprimir
    call mostrar_display1              ; Actualiza display1 con nuevo valor
    call mostrar_display2              ; Actualiza display2 (G/E/L)
    return                             ; Vuelve al principal
no_incrementa:
    clrf  hubo_cambio, 1               ; hubo_cambio = 0 (no cambió)
    return                             ; Vuelve al principal

decrementar_valor:
    movlb 5H                           ; Asegura banco de variables
    movf  valor_actual, 0, 1           ; W = valor_actual
    bz    no_decrementa                ; Si es 0, no decrementa
    decf  valor_actual, 1, 1           ; valor_actual = valor_actual - 1
    movlw 01H                          ; W = 1
    movwf hubo_cambio, 1               ; hubo_cambio = 1 (sí cambió)
    movlb 4H                           ; Banco SFR para imprimir
    call mostrar_display1              ; Actualiza display1
    call mostrar_display2              ; Actualiza display2
    return                             ; Vuelve al principal
no_decrementa:
    clrf  hubo_cambio, 1               ; hubo_cambio = 0 (no cambió)
    return                             ; Vuelve al principal

mostrar_display1:
    movlb 5H                           ; Banco de variables
    movf  valor_actual, 0, 1           ; W = índice 0..15
    movlb 4H                           ; Banco SFR (TBLPTR y LATD aquí)
    movwf TBLPTRL, 1                   ; TBLPTRL = 00H + índice (tabla_hex)
    TBLRD*                             ; TABLAT <- byte de tabla (patrón 7-seg)
    movf  TABLAT, 0, 1                 ; W = patrón leído
    movwf LATD, 1                      ; Muestra patrón en RD0..RD6
    return                             ; Fin de impresión display1

mostrar_display2:
    movlb 5H                           ; Banco de variables
    movf  valor_actual, 0, 1           ; W = valor_actual
    sublw 5                            ; W = 5 - valor
    bz    elegir_E                     ; Si W==0 -> valor==5 -> 'E'
    bnc   elegir_G                     ; Si (5 - valor) < 0 -> valor>5 -> 'G'
    movlw 2                            ; Si no, valor<5 -> índice 2 = 'L'
    bra   leer_gel                     ; Salta a lectura de tabla G/E/L
elegir_G:
    movlw 0                            ; Índice 0 = 'G'
    bra   leer_gel                     ; Salta a lectura
elegir_E:
    movlw 1                            ; Índice 1 = 'E'

leer_gel:
    addlw 10H                          ; 001000H + 10H = 001010H (tabla_gel)
    movlb 4H                           ; Banco SFR
    movwf TBLPTRL, 1                   ; Apunta a la entrada 'G'/'E'/'L'
    TBLRD*                             ; TABLAT <- patrón de letra
    movf  TABLAT, 0, 1                 ; W = patrón leído
    movwf LATF, 1                      ; Muestra letra en RF0..RF6
    return                             ; Fin de impresión display2

    end upcino                         

