all: cit citin flashprog modt ddtest setmod ddflash

cit: cit.c
	gcc -o cit cit.c -lpthread

modt: modt.c
	gcc -o modt modt.c -lpthread

setmod: setmod.c
	gcc -o setmod setmod.c -I../include/

flashprog: flashprog.c
	gcc -o flashprog flashprog.c

ddtest: ddtest.c
	gcc -o ddtest ddtest.c

ddflash: ddflash.c
	gcc -o ddflash ddflash.c
