#!/usr/bin/perl

while(<STDIN>){
   s/`/`x/g;

   # Automatically add file name and line number to log lines
   s/\\\"/`q/g;
   s/\$dlog([a-z]+)\("([^"]*?)"/\$dlog$1("[\%d]\%s(\%d): $2",(pid_t)syscall(SYS_gettid),__FILE__,__LINE__/g;
   s/`q/\\"/g;

   s/\\\$/`S/g;
   s/\$\$/ESFS_/g;
   s/\$/esfs_/g;
   s/`S/\$/g;

   s/`x/`/g;
   print;
}