PTHREAD_BINS=cit modt
INCLUDE_BINS=setmod setmod2 setmod3
STANDARD_BINS=flashprog ddtest ddflash pls

all: $(PTHREAD_BINS) $(INCLUDE_BINS) $(STANDARD_BINS)

$(PTHREAD_BINS): %: %.c
	gcc -o $@ $< -lpthread

$(STANDARD_BINS): %: %.c
	gcc -o $@ $<

$(INCLUDE_BINS): %: %.c
	gcc -o $@ $< -I../include
