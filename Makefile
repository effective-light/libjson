FLAGS = -Wextra -O2 -std=gnu99

main: main.o hashtable.o json.o
	gcc ${FLAGS} -o $@ $^

%.o: %.c
	gcc ${FLAGS} -c $<

clean:
	rm -f *.o main
