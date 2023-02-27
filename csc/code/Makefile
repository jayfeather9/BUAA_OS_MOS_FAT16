GXX=gcc
INCLUDE_DIR=../include

out: fibo.o main.o


.SUFFIXES: .c
.c.o:
	${GXX} -I${INCLUDE_DIR} -c $<

.PHONY: clean
clean:
	rm *.o

