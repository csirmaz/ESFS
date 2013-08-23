#!/usr/bin/perl

while(<STDIN>){
   s/`/`x/g;
   s/\\\$/`S/g;
   s/\$\$/ESFS_/g;
   s/\$/esfs_/g;
   s/`S/\$/g;
   s/`x/`/g;
   print;
}