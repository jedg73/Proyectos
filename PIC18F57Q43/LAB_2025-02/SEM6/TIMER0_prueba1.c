PROCESSOR 18F57Q43
#include "cabecera.inc"

    PSECT upcino, class=CODE, reloc=2, abs

upcino:
    ORG 000000H		;vector de reset
    bra configuro	;salto a etiqueta configuro
    
    ORG 000200H		;zona de programa de usuario

configuro:
    ;configuracion de la fuente de reloj
    movlb 0H		;me voy al bank 0
    movlw 60H		
    movwf OSCCON1, 1	;OSCCON1 60H
    movlw 02H
    movwf OSCFRQ, 1	;OSCFRQ=02H
    movlw 50H
    movwf OSCEN, 1	;OSCEN
    
    ;conf de PPS
    movlb 2H
    movlw 39H
    movwf RC5PPS, 1
    ;conf TMR0
    movlb 3H
    movlw 80H
    movwf T0CON0, 1	;TMR0 enabled, 8bit mode, poste 1:1
    movlw 84H
    movwf T0CON1, 1	;clksrc = LFINTOSC, sync, presc 1:64
    movlw 250
    movwf TMR0H, 1	;valor de ref de comp 250
    ;conf E/S
    movlb 4H
    bcf TRISC, 5, 1	;RC5 como salida
    bcf ANSELC, 5, 1	; RC5 como digital
    bcf ANSELB, 1, 1	; RB1 como digital
    bsf WPUB, 1, 1	;RB1 como pullup
    bcf TRISD, 6, 1	;RD6  como salida
    bcf ANSELD, 6, 1	;RD6 como digital
 
inicio:
    btfsc PORTB, 1, 1
    bra $-2
    btg LATD, 6, 1
    btfss PORTB, 1, 1
    bra $-2
    bra inicio
    
    
    end upcino
    
