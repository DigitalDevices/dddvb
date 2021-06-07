all: cit citin flashprog modt ddtest setmod ddflash setmod2 pls setmod3 modconfig ddinfo

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

modconfig: modconfig.c
	$(CC) -o modconfig modconfig.c -I../include/

%: %.c
	$(CC) $(CFLAGS) -I../ddbridge  -I../include/ $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -I../ddbridge -o $@ $<

