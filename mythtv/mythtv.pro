include (settings.pro)

TEMPLATE = subdirs

win32-msvc* {

    # Libraries without dependencies

    SUBDIRS  = external
    SUBDIRS += libs
    SUBDIRS += programs

} else {

    message( "MythTV.pro is only used for building on windows with Visual Studio 2010+." );
    message( "Run ./configure on your system." );

}

