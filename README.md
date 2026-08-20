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

CBM runs on Linux only: it reads `/proc/net/dev` and uses the `SIOCGIFADDR`
ioctl to inspect network interfaces. It compiles on other systems (e.g. macOS),
but cannot display traffic there.

To build and install, run the following commands:

    $ ./autogen.sh
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
