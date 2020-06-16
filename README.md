# splitfits

`splitfits` extracts headers and data from a FITS file. Extracted chunks are written sequentially (`*.part_{0,1,2,3,...}`). The `*.part_map` produced by this program during the extraction process can also be used to recombine the exploded file segments.

### Why didn't you use CFITSIO?

CFITSIO is a full-featured library and has quite a few system dependencies. `splitfits` extracts raw FITS data at logical boundaries within the file. Basic `fread()` and `fwrite()` calls are sufficient for this task.

# Compiling

```sh
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
$ make install
```

# Usage

```
usage: splitfits [-o DIR] [-c] {FILE(s)}
 Options:
   -h  --help       This message
   -c  --combine    Reconstruct original file using .part_map data
   -o  --outdir     Path where output files are stored
```

# Examples

## Splitting a FITS file

```sh
$ splitfits sample.fits
Map: ./sample.fits.part_map
Writing: ./sample.part_0
Writing: ./sample.part_1
Writing: ./sample.part_2
Writing: ./sample.part_3
```

## Recombining a FITS file
```sh
$ splitfits -c sample.fits.part_map
Reading: sample.part_0
Reading: sample.part_1
Reading: sample.part_2
Reading: sample.part_3
Writing: ./sample.fits
```

## Realistic example

```sh
$ ls -lrSF
total 4816
drwxr-xr-x 2 human human    4096 Jun 16 13:45 split/
drwxr-xr-x 2 human human    4096 Jun 16 13:45 combine/
-rw-r--r-- 1 human human  699840 Jun 16 13:45 sample1.fits
-rw-r--r-- 1 human human 4219200 Jun 16 13:45 sample2.fits

# Split files and store the results in the "split" directory
$ splitfits -o split sample*.fits
Map: split/sample1.fits.part_map
Writing: split/sample1.part_0
Writing: split/sample1.part_1
Writing: split/sample1.part_2
Writing: split/sample1.part_3
Map: split/sample2.fits.part_map
Writing: split/sample2.part_0
Writing: split/sample2.part_1
Writing: split/sample2.part_2
Writing: split/sample2.part_3

# Examine the split data
$ ls -l split/
total 4828
-rw-r--r-- 1 human human     109 Jun 16 13:46 sample1.fits.part_map
-rw-r--r-- 1 human human   23040 Jun 16 13:46 sample1.part_0
-rw-r--r-- 1 human human  642240 Jun 16 13:46 sample1.part_1
-rw-r--r-- 1 human human   28800 Jun 16 13:46 sample1.part_2
-rw-r--r-- 1 human human    5760 Jun 16 13:46 sample1.part_3
-rw-r--r-- 1 human human     114 Jun 16 13:46 sample2.fits.part_map
-rw-r--r-- 1 human human   11520 Jun 16 13:46 sample2.part_0
-rw-r--r-- 1 human human 4196160 Jun 16 13:46 sample2.part_1
-rw-r--r-- 1 human human    8640 Jun 16 13:46 sample2.part_2
-rw-r--r-- 1 human human    2880 Jun 16 13:46 sample2.part_3

# Reconstruct the files using -c and store them in the "combine" directory
$ splitfits -o combine -c split/*.part_map
Map: split/sample1.fits.part_map
Reading: split/sample1.part_0
Reading: split/sample1.part_1
Reading: split/sample1.part_2
Reading: split/sample1.part_3
Writing: combine/sample1.fits
Map: split/sample2.fits.part_map
Reading: split/sample2.part_0
Reading: split/sample2.part_1
Reading: split/sample2.part_2
Reading: split/sample2.part_3
Writing: combine/sample2.fits

# Examine the combined data
$ ls -l combine
total 4808
-rw-r--r-- 1 human human  699840 Jun 16 13:47 sample1.fits
-rw-r--r-- 1 human human 4219200 Jun 16 13:47 sample2.fits

# Check the results
$ (diff sample1.fits combine/sample1.fits && echo SAME) || echo DIFFERENT
SAME

$ (diff sample2.fits combine/sample2.fits && echo SAME) || echo DIFFERENT
SAME
```

# Known issues

## "identical begin/end offset"

When the primary header borders an `XTENSION` header with no data separating them, the loop responsible for setting the next `.part_N` file generates a zero-length file. This behavior is mitigated by automatically skipping over such segments as they are detected. Do not be alarmed, however, if you observe a gap in the `.part_N` file names (`.part_0`, ..., `.part_2`, `.part_3`). The correct number of files and their names are recorded in the `.part_map`.

