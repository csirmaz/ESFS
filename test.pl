#!/usr/bin/perl

=pod

  This script implements a basic test for the ESFS filesystem.

  This file is part of ESFS, a FUSE-based filesystem that supports snapshots.
  ESFS is Copyright (C) 2013 Elod Csirmaz
  <http://www.epcsirmaz.com/> <https://github.com/csirmaz>.

  ESFS is based on Big Brother File System (fuse-tutorial)
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>,
  and was forked from it on 21 August 2013.
  Big Brother File System can be distributed under the terms of
  the GNU GPLv3. See the file COPYING.
  See also <http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/>.

  Big Brother File System was derived from function prototypes found in
  /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  fuse.h is licensed under the LGPLv2.

  ESFS is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  ESFS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along
  with this program.  If not, see <http://www.gnu.org/licenses/>.

=cut

use strict;

$| = 1;

sub create_write {
   my $filename = shift;
   my $contents = shift;

   my $fh;
   open( $fh, '>', $filename ) || die "Cannot open \'$filename\': $!";
   print $fh $contents;
   close($fh);
}

sub append {
   my $filename = shift;
   my $contents = shift;

   my $fh;
   open( $fh, '>>', $filename ) || die "Cannot open \'$filename\': $!";
   print $fh $contents;
   close($fh);
}

sub delete_file {
   my $filename = shift;

   unlink $filename || die "Cannot delete \'$filename\': $!";
}

sub test_contents {
   my $filename = shift;
   my $contents = shift;

   my $fh;
   my $fcont;
   open( $fh, '<', $filename ) || die "Cannot open \'$filename\': $!";
   while(<$fh>) { $fcont .= $_; }
   close($fh);
   if( $fcont ne $contents ) {
      die "Testing \'$filename\' for contents \'$contents\' failed";
   }
}

sub test_nonexistent {
   my $filename = shift;

   if( -e $filename ) {
      die "Test failed: \'$filename\' should not exist";
   }
}

sub create_snapshot {
   my $name = shift;

   mkdir 'snapshots/' . $name || die "Cannot create snapshot \'$name\': $!";
}

sub delete_snapshot {
   rmdir 'snapshots' || die "Cannot delete snapshot: $!";
}

# Setup
#######

if( -e 'test' ) {
   die "'test' already exists - cannot continue";
}
mkdir 'test'      || die "Setup failed";
mkdir 'test/data' || die "Setup failed";
mkdir 'test/mnt'  || die "Setup failed";
print `./esfs test/data test/mnt`;
chdir 'test/mnt' || die "Cannot chdir";

# Test
######

test_nonexistent('file1');
test_nonexistent('file2');

create_write( 'file1', 'Hello' );

create_snapshot('s1');

create_write( 'file2', 'Two' );
append( 'file1', ' world' );

test_contents( 'snapshots/s1/file1', 'Hello' );
test_nonexistent('snapshots/s1/file2');
test_contents( 'file1', 'Hello world' );
test_contents( 'file2', 'Two' );

create_snapshot('s2');

delete_file('file1');

test_contents( 'snapshots/s1/file1', 'Hello' );
test_nonexistent('snapshots/s1/file2');
test_contents( 'snapshots/s2/file1', 'Hello world' );
test_contents( 'snapshots/s2/file2', 'Two' );
test_nonexistent('file1');
test_contents( 'file2', 'Two' );

create_write( 'file1', 'Second' );
append( 'file1', ' test' );

test_contents( 'snapshots/s1/file1', 'Hello' );
test_nonexistent('snapshots/s1/file2');
test_contents( 'snapshots/s2/file1', 'Hello world' );
test_contents( 'snapshots/s2/file2', 'Two' );
test_contents( 'file1',              'Second test' );
test_contents( 'file2',              'Two' );

delete_snapshot();
delete_snapshot();

# Cleanup
#########

sleep 3;

chdir '../..' || die "Cannot chdir";
`fusermount -u test/mnt`;
rmdir 'test/mnt' || die "Error: test/mnt is not empty";
`rm -rf test`;

print "Done - no errors\n";
