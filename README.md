ESFS
====

A FUSE-based filesystem that supports snapshots

ESFS uses an underlying filesystem to provide support for snapshots. It has the following restrictions to make the maintenance of snapshots easier:

- Snapshots are read-only
- Only the earliest snapshot can be deleted
- The perfomrance os reading snapshots does not need to be good
- Special nodes (FIFO, etc.) and hard links are not allowed

It stores snapshots in the underlying and virtual directory "snapshots/". It mainly forwards any request to read or write a node outside this directory, in the main space, to the underlying filesystem.
