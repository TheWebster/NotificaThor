Changelog
=========

## 0.3.1
* Added three new themes
* Fixed custom position drawing
* Fixed symbol parsing (an unknown symbol would cause a segfault)
* Added missing 'lighten' operator
* Cleaned source code

## 0.3.0
* Corrected help message for notificathor
* Corrected man-page for thor-cli
* New cleaner build system
* Worked over drawing subroutines
* Removed 'none' border-type
* New 'instance-already-running'-system
* Added inotify-watch on themefile.
* thor-cli can now submit icons directly
* Abandoned concept of 'popups'
* '--no-image' and '--no-bar' option for thor-cli
* notificathor now returns 0 on SIGINT and SIGTERM

## 0.2.3
* Added sections about comments in NotificaThor-themes(5)
* Expanded example configuration by volume adjustment
* Contents are now rendered to a buffer first and then to the window to prevent flickering
* Introduced cache to store png surfaces for performance

## 0.2.2
* Bugfix: Error while handling inotify events

## 0.2.1
* Bugfix: bars with rounded corners are now displayed correctly

## 0.2.0
* Added support for SHAPE extension
* Now monitoring config file and reread it upon change

## 0.1.4
* Added basic fallback if there is no themefile
* Added more compositing operators
* Added example script for showing brightness

## 0.1.3
* Fixed drawing subroutines for bars with rounded corners
* Revised config parsing
* Newline after version string

## 0.1.2
* Corrected invalid memory usage for theme names
* Removed config reload on SIGHUP
* Now a border can be drawn around the whole bar
* osd_default_timeout in rc.conf can be a real number
* Added nice configuration examples
* Revised build system for simpler version control

## 0.1.1
* Centralized X event loop
* NotificaThor now exits gracefully when X Server is closed
* Updated README

## 0.1.0
* Removed remains of old --config option
* Added descriptive headers to sourcefiles
