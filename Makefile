FLAGS = -Wextra -O2 -std=gnu99 -lm

main: main.o hashtable.o libjson.o
	gcc ${FLAGS} -o $@ $^

%.o: %.c
	gcc ${FLAGS} -c $<

clean:
	rm -f *.o main
