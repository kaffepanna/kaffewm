CC=cc
LD=gcc
CFLAGS=-Wall -g $(shell pkg-config --cflags x11)
LDFLAGS=$(shell pkg-config --libs x11)

tinywm: main.o
	$(LD) $< $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.c: indent
indent: main.c
	indent -nbad -bap -nbc -bbo -hnl -br -brs -c33 -cd33 -ncdb -ce -ci4 \
	-cli0 -d0 -di1 -nfc1 -i8 -ip0 -l80 -lp -npcs -nprs -npsl -sai \
	-saf -saw -ncs -nsc -sob -nfca -cp33 -ss -ts8 -il1 $<

clean:
	rm -f *.o tinywm
