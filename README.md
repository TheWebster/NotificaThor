NotificaThor
============

Description
-----------

Highly configurable on screen displays for X.


When running a minimal desktop environment with openbox and the likes
on a notebook, one might miss those fancy popups that appear when you
change brightnes and volume levels, eject a disk, etc...
NotificaThor allows you to do this.

NotificaThor comes as a simple user daemon, that can be put into .xinitrc
or whatever startup method you prefer.
You can then send commands to it with a simple commandline client.


Install instructions
--------------------

Build dependencies:
- libxcb
- libcairo
- librt
- libpthread
- libxcb-shape

Simply run 'make install'.


Example config files are provided in the config directory.
