#!/usr/bin/perl

while(<STDIN>){
   s/`/`x/g;

   # Automatically add the thread number, file name and line number to log lines.
   # See $dlogi, $dlogdbg and $_log in util.c
   s/\\\"/`q/g;
   s/\$dlog([a-z]+)\("([^"]*?)"/\$dlog$1(fsdata,"\%-20s(\%03d): $2",__FILE__,__LINE__/g;
   s/`q/\\"/g;

   s/\\\$/`S/g;
   s/\$\$/ESFS_/g;
   s/\$/esfs_/g;
   s/`S/\$/g;

   s/`x/`/g;
   print;
}