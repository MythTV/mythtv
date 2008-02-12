include ( ../config.mak )
include ( ../settings.pro )

TEMPLATE=subdirs

using_bindings_perl {
    SUBDIRS+=perl
}

using_bindings_python {
    SUBDIRS+=python
}
