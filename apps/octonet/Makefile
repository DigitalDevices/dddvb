all: ddtest octonet octokey ddflash octoscan

install: all
	install -m 0755 ddtest $(DESTDIR)/usr/bin
	install -m 0755 octonet $(DESTDIR)/usr/bin
	install -m 0755 octokey $(DESTDIR)/usr/bin
	install -m 0755 ddflash $(DESTDIR)/usr/bin
	install -m 0755 octoscan $(DESTDIR)/usr/bin

ddflash: ddflash.c
	$(CC) -o ddflash ddflash.c

ddtest: ddtest.c
	$(CC) -o ddtest ddtest.c

octonet: octonet.c
	$(CC) -o octonet octonet.c

octokey: octokey.c
	$(CC) -o octokey octokey.c

octoscan: octoscan.c
	$(CC) -o octoscan octoscan.c
