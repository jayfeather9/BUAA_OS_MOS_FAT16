.PHONY: all clean

all: test

test: test.c
	gcc test.c -o test

run: test
	./test

clean:
	rm test

