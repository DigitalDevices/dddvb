all: cit citin flashprog modt ddtest setmod ddflash setmod2 pls setmod3

cit: cit.c
	$(CC) -o cit cit.c -lpthread

modt: modt.c
	$(CC) -o modt modt.c -lpthread

setmod: setmod.c
	$(CC) -o setmod setmod.c -I../include/

setmod2: setmod2.c
	$(CC) -o setmod2 setmod2.c -I../include/

setmod3: setmod3.c
	$(CC) -o setmod3 setmod3.c -I../include/

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

