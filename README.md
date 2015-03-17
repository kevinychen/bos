# BOS
Filesystem with backup and retrieval

Goals
-----

The goal of this project is to extend the JOS operating system to support automatic
snapshotting and recording of all file modifications. A simple shell interface allows
users to access any file in a previous time-point, and revert back the file if desired.

Such a system has the following benefits:

1. Users can easily view files at an earlier point in time;

2. Users can easily access previous versions programatically, e.g. compile previous
versions, diff with previous versions, and search for keywords in all previous versions;

3. Users can retrieve files that they have accidentally deleted or overwritten;

4. The snapshotting is automatic, so BOS provides convenient backups.

Interface
---------

The shell syntax for reading and writing the most recent version of a file remains
unchanged. However, it will also be possible to access previous versions of files, using
the syntax `file@time`:

    $ cat document.txt@3-1-2015,6:52PM

The time-point does not necessarily refer to a time when the file was modified. BOS will
retrieve the version of the file present at that time, i.e. the version that was most
recently modified before the specified time-point.

Shortcuts can also be implemented for common types of accesses:

    $ diff documentxt.txt@yesterday document.txt@last-week

Note that the file name is only `document.txt`; the time after the `@` is
translated by the shell to a separate argument used when accessing the file.

Finally, the user can look up file modification times with a history query:

    $ history document.txt
