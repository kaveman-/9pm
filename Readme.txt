This directory contains a Windows port of a number of plan 9 utilities,
including an updated version of the sam text editor. This version of
sam supports the -r option using ssh as the remote execution protocol.

Source is included.  It should be possible to rebuild the
tools from the source, but it will probably require some work.
In particular, the source is mostly built using version 4.2 of
Visual C++. The source was ported using a plan 9 emulation
library called 9pm, which completely replaces the standard
libc library included with the compilers.

The remote execution command srx and the libraries mp and libsec
are compiled with Version 6.0.  This source does not use the 9pm library.

Most of these tools are rather out of date.  Only a small number of changes
have been made since 1997.  Maintenance and updating of these tools is
left as an exercise to the reader.

Note: this code is released under the Plan 9 open source license agreement.

Enjoy
Sean Quinlan
