.rodata
.balign 32
.globl tam_fondo
.globl fondodata

tam_fondo:	.long	picdataend - fondodata
fondodata:
.incbin "../source/test.jpg"
picdataend:


