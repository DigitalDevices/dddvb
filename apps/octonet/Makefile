PROGS=ddtest octonet octokey ddflash
all: $(PROGS)

install: $(subst,$(PROGS),%,install-%)
$(subst,$(PROGS),%,install-%): install-%: %
	install -m 0755 $< $(DESTDIR)/usr/bin

$(PROGS): %: %.c
	$(CC) -o $@ $<
