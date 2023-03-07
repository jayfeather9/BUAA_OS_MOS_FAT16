%.o: %.c
	$(CC) -c $<

.PHONY: clean run

readelf: main.o readelf.o
	$(CC) $^ -o $@

hello: hello.c
	$(CC) $^ -o $@ -m32 -static -g

run: readelf hello
	./readelf hello

clean:
	rm -f *.o readelf hello
