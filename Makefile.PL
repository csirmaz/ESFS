use strict;

my $m = <<THEEND
esfs : esfs_c.o log_c.o
	gcc -O3 -o esfs esfs_c.o log_c.o `pkg-config fuse --libs`

esfs_c.o : esfs_c.c log_c.h params_c.h snapshot_c.c util_c.c stat_c.c fuse_fd_close_c.c fuse_fd_read_c.c fuse_fd_write_c.c fuse_path_open_c.c fuse_path_read_c.c fuse_path_write_c.c
	gcc -O3 -Wall `pkg-config fuse --cflags` -c esfs_c.c

log_c.o : log_c.c log_c.h params_c.h
	gcc -O3 -Wall `pkg-config fuse --cflags` -c log_c.c

THEEND
;


foreach my $f (qw( esfs.c params.h log.c log.h snapshot.c util.c stat.c fuse_fd_close.c fuse_fd_read.c fuse_fd_write.c fuse_path_open.c fuse_path_read.c fuse_path_write.c )){
my $fdest = $f;
$fdest =~ s/\./_c./;
$m .= <<THEEND
$fdest : $f
	./compile.pl < $f > $fdest

THEEND
;
}

$m .= <<THEEND
clean:
	rm -f esfs *.o *_c.c *_c.h Makefile
THEEND
;

my $old = <<THEEND
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
THEEND
;

open (OUTFILE, '>Makefile');
print OUTFILE $m;
close (OUTFILE);
