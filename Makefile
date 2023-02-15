.PHONY: clean

out: calc case_all
	./calc < case_all > out

# Your code here.
casegen: casegen.c
	gcc casegen.c -o casegen
	chmod +x casegen

calc: calc.c
	gcc calc.c -o calc
	chmod +x calc

case_all: case_add case_sub case_mul case_div
	cat case_add case_sub case_mul case_div > case_all

case_%: casegen
	./casegen $* 100 > case_$*

clean:
	rm -f out calc casegen case_*
