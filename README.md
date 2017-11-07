# PinguescoFS

PinguescoFS was one of the university projects for the operating systems
course, back in 2011.  You can find the report [here](report/report.pdf).

# Running PinguescoFS

The steps to get pinguescoFS up and running are the following:

1. Create a device
   To test pinguescoFS a device must be created. This can be a regular
   file for test purposes. To do so create a large file with dd:

```
   $ dd if=/dev/zero of=<dev> count=4096 bs=1MB
```

   Note that other sizes are supported by the file system. The restrictions
   are that the device must not have more than 4GB and must have more than
   ~16MB, the size of the file allocation table.

2. Format the device
   After compiling the pinguescoFS (see [INSTALL.md](INSTALL.md)) enter the
   formatter directory and run

```
   $ ./formatter <dev>
```

3. Mount the filesystem and enter the `src/` directory and run 

```
   $ ./pinguescofs <mount_point> <dev>
```
