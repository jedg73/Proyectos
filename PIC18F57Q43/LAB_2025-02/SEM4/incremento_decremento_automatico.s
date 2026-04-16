;se usan los puertos A y F
    PROCESSOR 18F57Q43
    #include "cabecera.inc" 
    
X            EQU 0500H
Y            EQU 0501H
Z            EQU 0502H
valor_actual EQU 0503H

    PSECT upcino, class=CODE, reloc=2, abs

upcino:
    ORG 000000H
    bra configuro

    ORG 001000H

tabla_hex:
    DB 3FH, 06H, 5BH, 4FH, 66H, 6DH, 7DH, 07H
    DB 7FH, 67H

    ORG 000100H

configuro:
    movlb 0H
    movlw 60H
    movwf OSCCON1,1
    movlw 02H
    movwf OSCFRQ,1; 4MHz
    movlw 40H
    movwf OSCEN,1

    movlb 4H

    movlw 80H ;1000 0000 se le asigna a w
    movwf TRISA,1
    movwf ANSELA,1
    clrf  LATA,1

    movlw 80H
    movwf TRISF,1
    movwf ANSELF,1
    clrf  LATF,1

    clrf  TBLPTRU,1
    movlw 10H
    movwf TBLPTRH,1
    clrf  TBLPTRL,1

    movlb 5H
    clrf  valor_actual,1

principal:
    movlb 4H
    call mostrar_displayA
    call mostrar_displayF
    call retardo_500ms

    movlb 5H
    incf  valor_actual,1,1

    movf  valor_actual,0,1
    xorlw 0AH
    bz    reiniciar
    bra   principal

reiniciar:
    clrf  valor_actual,1
    bra   principal



; PORTA = 0,1,2,3,4,5,6,7,8,9

mostrar_displayA:
    movlb 5H
    movf  valor_actual,0,1

    movlb 4H
    movwf TBLPTRL,1
    TBLRD*
    movf  TABLAT,0,1
    movwf LATA,1
    return



; PORTF = 9,8,7,6,5,4,3,2,1,0

mostrar_displayF:
    movlb 5H
    movf  valor_actual,0,1
    sublw 09H

    movlb 4H
    movwf TBLPTRL,1
    TBLRD*
    movf  TABLAT,0,1
    movwf LATF,1
    return


;-----------------------------------------
; Retardo ~500 ms
; 16 * 100 * 52 * 6us = 499200us
;-----------------------------------------
retardo_500ms:
    movlb 5H

    movlw 34H        ; 52
    movwf Z,1

ZZ:
    movlw 64H        ; 100
    movwf Y,1

YY:
    movlw 10H        ; 16
    movwf X,1

XX:
    decfsz X,1,1
    bra XX
    decfsz Y,1,1
    bra YY
    decfsz Z,1,1
    bra ZZ

    movlb 4H
    return

    end upcino
