
CC := gcc
main: main.o process.o
	gcc  main.o  process.o -o main
main.o: main.c Makefile
	gcc main.c -c
process.o: process.c process.h Makefile
	gcc process.c -c
clean:
	rm -rf *o
# run:
# 	sudo ./main
