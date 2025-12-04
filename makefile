penc: main.c
	cc -o penc main.c

install: penc penc.1
	mkdir -p /usr/local/bin
	cp penc /usr/local/bin
	mkdir -p /usr/local/share/man/man1
	cp penc.1 /usr/local/share/man/man1

test: penc
	./test.sh

clean: penc
	rm penc
