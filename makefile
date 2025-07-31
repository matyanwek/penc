.PHONY: all

all:
	cc -o penc penc.c

.PHONY: install

install:
	mkdir -p /usr/local/bin /usr/local/share/man/man1
	cc -o penc penc.c
	cp penc /usr/local/bin
	cp penc.1 /usr/local/share/man/man1

.PHONY: test

test:
	cc -o penc penc.c
	./test.sh
