include (../settings.pro)

TEMPLATE = subdirs

win32-msvc* {

# Libraries without dependencies

!using_exiv2_external: SUBDIRS += libexiv2
!using_libbluray_external: SUBDIRS += libmythbluray
SUBDIRS += libmythdvdnav
!using_libudfread_external: SUBDIRS += libudfread

}
