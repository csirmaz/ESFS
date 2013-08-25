ESFS
====

A FUSE-based filesystem that supports snapshots

ESFS uses an underlying filesystem to provide support for snapshots. Its primary aim was to provide snapshots for database files. It has the following restrictions to make its performance better and the maintenance of snapshots easier:

- Snapshots are read-only
- Only the earliest snapshot can be deleted
- The perfomrance os reading snapshots does not need to be good
- Special nodes (FIFO, etc.) and hard links are not allowed

