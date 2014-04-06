ESFS
====

ESFS is a FUSE-based filesystem that supports snapshots

ESFS can maintain a series of read-only snapshots, and is optimised for speed
when reading or writing the current versions of the files.
It uses an underlying filesystem to carry out the file operations,
and to save the data necessary to maintain the snapshots themselves.

Please see
http://www.epcsirmaz.com/?q=esfs-snapshotting-filesystem
and
https://github.com/csirmaz/ESFS/wiki
for information and installation instructions.
