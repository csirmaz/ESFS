ESFS
====

A FUSE-based filesystem that supports snapshots

ESFS is a filesystem which can maintain a series of read-only snapshots, and which is optimised for speed when accessing the current versions of the files. It uses an underlying filesystem to store data for the snapshots, and to carry out operations after saving any modified data.
