esfs : esfs.o log.o
	gcc -g -o esfs esfs.o log.o `pkg-config fuse --libs`

esfs.o : esfs.c log.h params.h
	gcc -g -Wall `pkg-config fuse --cflags` -c esfs.c

log.o : log.c log.h params.h
	gcc -g -Wall `pkg-config fuse --cflags` -c log.c

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
