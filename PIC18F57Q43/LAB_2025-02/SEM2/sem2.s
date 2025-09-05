;Profesor Kalun José Lau Gan
;Alumno Joaquin Davila Garcia 
;Universidad Peruana de Ciencias aplicada (UPC)
;Lenguaje usado Assembler
;Desarrollo de una compuerta NOT usando el PIC18F57Q43 usando los pines RA0 para el pulsador y RD0 para el LED

PROCESSOR 18F57Q43
#include "cabecera.inc"

    PSECT upcino, class=CODE, reloc=2, abs

upcino:
    ORG 000000H		;vector de reset
    bra configuro	;salto a etiqueta configuro
    
    ORG 000100H		;zona de programa de usuario

configuro:
    ;configuracion de la fuente de reloj
    movlb 0H		;me voy al bank 0
    movlw 60H		
    movwf OSCCON1, 1	;OSCCON1 60H
    movlw 02H
    movwf OSCFRQ, 1	;OSCFRQ=02H
    movlw 40H
    movwf OSCEN, 1	;OSCEN=40H
    
    ;configuracion de las E/S:
    movlb 4H		;me voy al bank 4
    bsf TRISA, 0, 1	;RA0 como entrada
    bcf ANSELA, 0, 1	;RA0 como digital
    bcf TRISD, 0, 1	;RD0 como salida
    bcf ANSELD, 0, 1	;RD0 como digital
    
inicio:
    btfss PORTA, 0, 1	;pregunta si RA0 es uno
    bra falsote		;viene aqui cuando es falso, salta a la etiqueta falsote
    bra deveritas	;viene aqui cuando es verdadero, salta a la etiqueta deveritas

falsote:
    bsf LATD, 0, 1	;salida RD0 a uno logico
    bra inicio		;regresa a la etiqueta inicio
    
deveritas:
    bcf LATD, 0, 1	;salida RD0 como 0 logico
    bra inicio		;regresa a la etiqueta de inicio
    
    end upcino

