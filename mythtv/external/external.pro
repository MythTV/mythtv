include (../settings.pro)

TEMPLATE = subdirs

win32-msvc* {

# Libraries without dependencies

SUBDIRS += libhdhomerun
SUBDIRS += libmythbluray
SUBDIRS += libmythdvdnav
SUBDIRS += libsamplerate
SUBDIRS += qjson/src
SUBDIRS += nzmqt/src/nzmqt.pro

}