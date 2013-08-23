esfs : esfs.o log.o
	gcc -O3 -o esfs esfs.o log.o `pkg-config fuse --libs`

esfs.o : esfs_c.c log_c.h params_c.h snapshot_c.c util_c.c
	gcc -O3 -Wall `pkg-config fuse --cflags` -c esfs_c.c

log.o : log_c.c log_c.h params_c.h
	gcc -O3 -Wall `pkg-config fuse --cflags` -c log_c.c

esfs_c.c : esfs.c
	./compile.pl < esfs.c > esfs_c.c

params_c.h : params.h
	./compile.pl < params.h > params_c.h

log_c.c : log.c
	./compile.pl < log.c > log_c.c

log_c.h : log.h
	./compile.pl < log.h > log_c.h

snapshot_c.c : snapshot.c
	./compile.pl < snapshot.c > snapshot_c.c

util_c.c : util.c
	./compile.pl < util.c > util_c.c
	
clean:
	rm -f esfs *.o

dist:
	rm -rf fuse-tutorial/
	mkdir fuse-tutorial/
	cp ../*.html fuse-tutorial/
	mkdir fuse-tutorial/example/
	mkdir fuse-tutorial/example/mountdir/
	mkdir fuse-tutorial/example/rootdir/
	echo "a bogus file" > fuse-tutorial/example/rootdir/bogus.txt
	mkdir fuse-tutorial/src
	cp Makefile COPYING COPYING.LIB *.c *.h fuse-tutorial/src/
	tar cvzf ../../fuse-tutorial.tgz fuse-tutorial/
	rm -rf fuse-tutorial/
