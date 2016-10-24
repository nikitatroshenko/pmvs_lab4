all: sqlfs.out

sqlfs.out: sqlfs.o sqlite3.o
	gcc sqlite3.o sqlfs.o -o sqlfs.out -ldl -lfuse -lm -lpthread

sqlfs.o: sqlite_fs.c
	gcc -c -o sqlfs.o sqlite_fs.c

sqlite3.o: sqlite3.c
	gcc -c -o sqlite3.o sqlite3.c

clean:
	rm -f *.o *.out