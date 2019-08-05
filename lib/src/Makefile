LIB_FLAGS = -fvisibility=hidden -fPIC  -DBUILDING_LIBDDDVB

all: libdddvb.so.1.0.1 

%.o: %.c
	$(CC) $(LIB_FLAGS) $(CFLAGS) -c $< 

libdddvb.a: dvb.o dddvb.o tools.o config.o ca.o
	$(AR) -cvq libdddvb.a $^

libdddvb.so.1.0.1: dvb.o dddvb.o tools.o config.o ca.o
	$(CC) $(LIB_FLAGS) $(CFLAGS) -shared -Wl,-soname,libdddvb.so.1 -o libdddvb.so.1.0.1 $^ -lc
	ln -sf libdddvb.so.1.0.1 libdddvb.so.1 
	ln -sf libdddvb.so.1.0.1 libdddvb.so

dddvb_test: dddvb_test.o
	$(CC) -o dddvb_test  $< -L . -l dddvb

clean:
	-rm -f *.o
