# SPDX-License-Identifier: LGPL-3.0-or-later

FLAGS = -Wextra -O2 -std=gnu99 -lm -fpic

main: main.o hashtable.o libjson.o
	gcc ${FLAGS} -o $@ $^

%.o: %.c
	gcc ${FLAGS} -c $<

install: libjson.o hashtable.o
	gcc ${FLAGS} -shared -o libjson.so $<
	cp libjson.so /usr/lib

uninstall:
	rm -f /usr/lib/libjson.so

clean:
	rm -f *.so *.o main
