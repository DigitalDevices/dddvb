all:
	make -C ./src
	make ddzap

install: all
	cp -d src/libdddvb.so* /usr/local/lib
	cp -d src/libdddvb.h /usr/local/include/
	cp -d src/dddvb.h /usr/local/include/
	cp -d ddzap /usr/local/bin
	ldconfig

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $< 

ddzap: ddzap.o
	$(CC) -o ddzap  $< -L ./src -l dddvb -l pthread -l dvben50221 -l dvbapi -l ucsi -lm

clean:
	make -C ./src clean
	-rm -f *.o
