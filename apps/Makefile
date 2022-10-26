TARGETS = cit citin flashprog modt ddtest setmod ddflash setmod2 pls setmod3 modconfig ddinfo getiq modtest test.ts

all: $(TARGETS) 

CFLAGS =  -g -Wall -Wno-unused  -Wno-format
FFMPEG := $(shell command -v ffmpeg 2> /dev/null)

modtest: modtest.c
	$(CC) -o modtest modtest.c -I../include/ -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

test.ts:
ifndef FFMPEG
	$(error "ffmpeg is not available please install to create test.ts")
endif
	ffmpeg -f lavfi -i testsrc=duration=10:size=1280x720:rate=30  \
	-f lavfi -i sine=f=440:b=4  -shortest   -metadata \
	service_provider="DD"      -metadata service_name="Test" test.ts

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

clean:
	rm cit citin flashprog modt ddtest setmod ddflash setmod2 pls setmod3 modconfig ddinfo getiq

%: %.c
	$(CC) $(CFLAGS) -I../ddbridge  -I../include/ $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -I../ddbridge -o $@ $<


clean:
	for f in $(TARGETS) *.o *~ ; do \
		if [ -e "$$f" ]; then \
			rm "$$f" || exit 1; \
		fi \
	done
