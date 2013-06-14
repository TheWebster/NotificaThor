NotificaThor
============

Description
-----------

Themeable On Screen Displays for X.

When running a minimal desktop environment with openbox and the likes
on a notebook, one might miss those fancy popups that appear when you
change brightnes and volume levels, eject a disk, etc...
NotificaThor allows you to do this.

NotificaThor comes as a simple user daemon, that can be put into .xinitrc
or whatever startup method you prefer.
You can then send commands to it with a simple commandline client.

NotificaThor aims to be a more flexible and aesthetic alternative to the trusty old XOSD.


Dependencies
------------

- GNU C Compiler
- GNU Make
- librt
- libpthread
- libxcb
- libxcb-shape
- libfreetype2
- libfontconfig
- libmath
- libcairo >= 1.12
    with support for *fontconfig*, *freetype*, *png-functions*, *image-surfaces* and *xcb-surfaces*.


Installing
----------

Simply running

	make install
as root will install the binaries to */usr/bin*, configuration to */etc/NotificaThor* and MAN-pages to */usr/share/man/*.
If you want to install to a fake root directory (e.g. for package creation) use the *prefix*-variable.

	make install prefix=/fake/root
The subdirectories *usr/bin*, *usr/share/man* and *etc* must exist.

Debugging
---------

	make debug
Compiles source files without optimizations and with debugging informations (-O0 -g).

	make testing
Compiles source files with the TESTING-flag defined. NotificaThor will then only read config files from ./etc/NotificaThor/.

	make verbose
Compiles source files with the VERBOSE-flag defined. This activates some more debugging messages when the -v option is used.

The commands can be combined:

	make debug testing verbose

Usage
-----

After installing run

	$ notificathor
then for producing an OSD run

	$ thor-cli -b2/3 --no-image
This will produce a plain popup with a bar filled to 2/3 (note the astounding resemblance) and no image.
To add an image run

	$ thor-cli -b2/3 -i"/path/to/an/image-file"

You can also show a text message:

	$ thor-cli --no-image --no-bar -m'Hello World!'
