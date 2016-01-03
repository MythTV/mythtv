include (../settings.pro)

TEMPLATE = subdirs

win32-msvc* {

# Libraries without dependencies

SUBDIRS += libhdhomerun
SUBDIRS += libmythbluray
SUBDIRS += libmythdvdnav
SUBDIRS += libsamplerate
SUBDIRS += libudfread
SUBDIRS += nzmqt/src/nzmqt.pro
SUBDIRS += minilzo
SUBDIRS += libmythsoundtouch

}
