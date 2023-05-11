TARGETS = cit ddtest setmod1 setmod2 modconfig ddinfo getiq modtest

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

%: %.c
	$(CC) $(CFLAGS) -I../ddbridge  -I../include/ $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -I../ddbridge -o $@ $<

clean:
	rm test.ts -f
	for f in $(TARGETS) *.o *~ ; do \
		if [ -e "$$f" ]; then \
			rm "$$f" || exit 1; \
		fi \
	done
