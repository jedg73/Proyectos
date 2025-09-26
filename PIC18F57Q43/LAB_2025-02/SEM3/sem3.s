;Alumno Joaquin Davila Garcia 
;Universidad Peruana de Ciencias aplicadas (UPC)
;Lenguaje usado Assembler

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
    
    movlb 04H
    movlw 0F0H
    movwf TRISD, 1	;RD0 al RD3 como salidas 
    movwf ANSELD, 1	;RD0 al RD3 como digitales
    bcf	  TRISB, 0, 1	;RB0 como salida
    bcf	  ANSELB, 0 ,1	;RB0 como digital
    setf  TRISF, 1	;Todo el puerto F como entradas
    movlw 0FCH
    movwf ANSELF,1	;RF1 y RF0 como digitales
    movlw 03H
    movwf WPUF, 1	;RF1 y RF0 como pullup activadas
    
inicio:
    btfsc PORTF, 0, 1	;Pregunta si RF0 es cero (si presiona el boton1)
    bra	  preguntaRF1	;falso, salta a etiqueta pregunta RF2
    btg	  LATB, 0, 1	;verdad, aplica complemento a RB0

otro1:
    btfss PORTF, 0, 1	;Pregunta si RF0 es uno (si solte el boton)
    bra otro1		;falso, regresa a pregunta RF=1
    bra inicio		;verdad, imprimir A en RD
    
preguntaRF1:
    btfsc PORTF,1 ,1	;Pregunta si RF1 es cero(si se presiona el boton2)
    bra imprime5	;falso, salta a label imprime5
    movlw 0AH		;verdad, a imprimir A en RD
    movwf LATD,1
    bra inicio
    
imprime5:
    movlw 05H		;imprimir 5 en RD
    movwf LATD, 1 
    bra inicio		;regresa a inicio
    
    end upcino

