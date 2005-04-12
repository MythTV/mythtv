TEMPLATE = subdirs

include (config.pro)

!exists( config.pro ) {
   error(Missing config.pro: please run the configure script)
}

