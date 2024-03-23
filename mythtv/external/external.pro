include (../settings.pro)

TEMPLATE = subdirs

win32-msvc* {

# Libraries without dependencies

!using_system_libexiv2: SUBDIRS += libexiv2
!using_system_libbluray: SUBDIRS += libmythbluray
SUBDIRS += libmythdvdnav
!using_system_libudfread: SUBDIRS += libudfread

}
