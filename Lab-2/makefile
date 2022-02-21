all: jdisk.o FATRW

clean:
	rm *.o FATRW

jdisk.o: jdisk.c
	gcc -c -o jdisk.o jdisk.c

FATRW: FATRW.o jdisk.o
	gcc -o FATRW FATRW.c jdisk.o
