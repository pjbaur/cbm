# CBM (Color Bandwidth Meter)

#### cbm - display in real time the network traffic speed

> **Fork note:** this is a personal fork of CBM. It carries local fixes and
> experiments ahead of the upstream project, which lives at
> https://github.com/resurrecting-open-source-projects/cbm

## Help this project ##

CBM needs your help. **If you are a programmer** and if you wants to help a
nice project, this is your opportunity.

CBM was imported from some tarballs from Debian Project[1].
After this, all patches found in Debian project and other places for this program
were applied. All initial work was registered in ChangeLog file (version 0.2 and
later releases). CBM is packaged in Debian[2] Project.

If you are interested to help CBM, read the [CONTRIBUTING.md](CONTRIBUTING.md) file.

[1] http://snapshot.debian.org/package/cbm/
[2] https://tracker.debian.org/pkg/cbm

## What is CBM? ##

The Color Bandwidth Meter (CBM) is a small program to display the traffic
currently flowing through the network devices in a simple curses-based GUI.
The traffic for all interfaces include values as receive, transfer and total
Bytes/s or bits/s (or its multiples as KB/s and Kb/s).

It is useful for Internet or LAN speed tests, measuring the velocity of a
link, to establish a benchmark or to monitor your connections. CBM can be
used with virtual, wired or wireless networks.

Nowadays, CBM is maintained by volunteers.

## Build and Install ##

CBM depends of libncurses to build.

CBM runs on Linux and macOS. On Linux it reads `/proc/net/dev`; on macOS it
reads per-interface counters with the `sysctl(3)` `NET_RT_IFLIST2` interface,
and it looks up interface addresses with `getifaddrs(3)` on both systems. Note
that the macOS kernel reports byte counters to non-system programs in 1 KiB
steps and wrapped at 4 GiB; cbm copes with the wrap, but idle or very low
traffic can read as zero between steps.

To build and install, run the following commands:

    $ ./autogen.sh
    $ ./configure
    $ make
    # make install

On macOS, install the autotools first (`brew install autoconf automake
libtool`); `./autogen.sh` needs `autoreconf`, which is not installed by
default.
    $ ./configure
    $ make
    # make install

To return to pristine sources, use '$ ./autogen.sh clean'. To remove only the
build outputs, use '$ make distclean'.

On Debian systems you can use '# apt install cbm'.

## Interactive keys ##

While running, CBM can be controlled with the following keys:

* Up/Down or p/n: select an interface to show details about.
* q: exit the program.
* b: switch between bits per second and bytes per second.
* + and -: change the update interval by 100ms.

## AUTHOR ##

CBM was originally developed by Aaron Isotton <aaron@isotton.com> under GPL-2.

Currently, source code and newer versions are available at
https://github.com/resurrecting-open-source-projects/cbm
