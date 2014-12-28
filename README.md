# ESFS

ESFS is a FUSE-based snapshotting (versioning) filesystem for Linux

ESFS can maintain a series of read-only snapshots, and is optimised for speed
when reading or writing the current versions of the files.
It uses an underlying filesystem to carry out the file operations,
and to save the data necessary to maintain the snapshots themselves.

Through the snapshots, it provides read-only access to previous states
of the whole filesystem, which can be used to restore data following
some corruption caused by human error or software bug.
In this sense, it can provide a backup / archiving solution,
although on the same storage as
the main filesystem itself.

Although ESFS supports most of the main filesystem operations,
its main aim is to provide file-based containers
that are used as block (loop) devices for filesystems that need
versioning implemented.

## Using ESFS

Use the `esfs` binary to mount
the filesystem. It needs two directories:
an empty mount point where the filesystem will appear,
and another directory under which ESFS will store its data.
Run `esfs [ FUSE_AND_MOUNT_OPTIONS ] [--local-log] (DATA_DIRECTORY) (MOUNTPOINT)`
to mount the filesystem.
To get a list of the FUSE and mount options, run
`esfs -h`.

You can now use the filesystem by creating, writing or reading files under `(MOUNTPOINT)`.
ESFS stores its internal data under `(DATA_DIRECTORY)`. Do not write or modify files under
this directory
as that can break the ESFS filesystem
(except in special circumstances - please see below).

When the filesystem is mounted, a `(MOUNTPOINT)/snapshots` directory
is created. The snapshots are all represented as
directories under `snapshots/`.

To create a new snapshot from the current state of the
filesystem, use `mkdir (MOUNTPOINT)/snapshots/(SNAPSHOT_NAME)`.
To delete the earliest snapshot, simply run `rmdir (MOUNTPOINT)/snapshots`.

You can access files in the snapshots under `snapshots/(SNAPSHOT_NAME)/`.
For example, the version of the file `(MOUNTPOINT)/mydir/myfile` at the time
the snapshot named `Monday` was taken is at
`(MOUNTPOINT)/snapshots/Monday/mydir/myfile`.

To un-mount the ESFS filesystem, run
`fusermount -u (MOUNTPOINT)`.

ESFS will also create a log file if it was compiled with a positive `$$DEBUG` value
(see params.h).
The default location of the log is `/var/log/esfs.log`,
but if you use the `--local-log` argument,
ESFS will try to open the log file in the directory it was started in.

## Installation and troubleshooting

Download the release of the ESFS source you would like to install from
GitHub
[here](https://github.com/csirmaz/ESFS/releases).

If you haven't already, you will need to install the tools
`gcc`, `make` and `pkg-config`, as well as FUSE and its header files.
On Debian, all of these can be installed using
`apt-get update; apt-get install gcc make pkg-config fuse libfuse-dev`.

In the directory where the source files of ESFS are,
run `perl Makefile.PL` to generate the Makefile;
and run `make` to compile ESFS. A new executable file
called `esfs` will be generated.
Settings and constants are defined in <tt>params.h</tt> and
`types.h`.

Run `groups` as the user you want to run ESFS as;
if `fuse` is not listed, you may need
to add the user to the `fuse` group. As root, run `adduser (USERNAME) fuse`
to do this.

Start ESFS by running `esfs (UNDERLYING_ROOT_DIR) (MOUNTPOINT)`,
or see above for more options.

If you get a `failed to open /dev/fuse: Permission denied` error
when starting `esfs`, please make sure that `/dev/fuse`
is group readable/writeable and it is owned by the group `fuse`.
To make it group readable/writeable, run, as root, `chmod g+rw /dev/fuse`;
and to make it owned by the user `root` and the group `fuse`, run `chown root:fuse /dev/fuse`.

If you want ESFS to use `/var/log/esfs.log` as its log,
you may need to create this file first as root and change its ownership or permissions so that ESFS could write it.
Use `touch /var/log/esfs.log` to create the file, and
`chown (USERNAME):(USERNAME) /var/log/esfs.log` to change the owner to the user
ESFS will run as.

## How does it work?

ESFS forwards most requests to the underlying filesystem,
but it intercepts writes and other modifications,
and uses copy-on-write to save the blocks of old data
before they are actually overwritten.
It only saves a block once, when the given block is modified
for the first time after a snapshot was created.

    +--------------------------+
    |           User           |
    +--------------------------+
                  |
    +--------------------------+
    |           ESFS           |
    +--------------------------+
            |          |
    +-+-----------+-+--------+-+
    | | Snapshots | | Mirror | |
    | +-----------+ +--------+ |
    |  Underlying filesystem   |
    +--------------------------+

This way, it keeps a mirror of the files you are working on
in the underlying filesystem, and subsequent read and
write operations are almost as fast as if there were
no snapshots present.

In the underlying filesystem, the data saved for the snapshots
are kept in two types of files. The `.dat` files contain the blocks
saved, while the `.map` files contain file metadata and information
about which blocks have been saved, and where.
Please see the documentation in the source for details.

## Security considerations

Please note that ESFS accesses the underlying filesystem
as the user it is running as. Among other things, this means:

* Bugs in the code can be exploited to access other files
in the system readable or writeable by this user. Because of this,
ESFS aborts if it is started as root.

* Other processes running as the same user as ESFS can access
all underlying files ESFS creates to store the filesystem data.

Please also be aware of the security implications of all FUSE options you use.

## Bugs and limitations

By design, ESFS has the following limitations:

* Snapshots are read-only
* Only the earliest snapshot can be deleted
* Paths starting with `/snapshots` are reserved for the snapshots

The current version of ESFS has these additional limitations:

* Renaming is not supported
* Group permission bits are ignored when accessing files in the snapshots
* Symlinks and hard links are not supported
* Special files (other than files and directories) are not supported

Please see `fuse_path_write.c` for a plan for a path map
that would allow to store renames efficiently.

If you find a bug in ESFS, please let me know by submitting it on GitHub,
[here](https://github.com/csirmaz/ESFS/issues).

## Wiki

* [Importing and Exporting Data](https://github.com/csirmaz/ESFS/wiki/Importing-and-Exporting-Data) - if you want to initialise ESFS with existing data, or to remove the ESFS layer
* [Running MySQL on ESFS](https://github.com/csirmaz/ESFS/wiki/Running-MySQL-on-ESFS)
* [Improving Performance](https://github.com/csirmaz/ESFS/wiki/Improving-Performance) - including using files in ESFS as block (loop) devices for other filesystems

## Disclaimer and licence

If you find ESFS useful, and would like to extend or improve it,
please feel free to submit patches or fork the code.

ESFS is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

ESFS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  details.

ESFS is Copyright (C) 2013, 2014 Elod Csirmaz

ESFS is under heavy development. It is not suitable for mission-critical
applications.

## Links

* [FUSE](http://fuse.sourceforge.net/)

