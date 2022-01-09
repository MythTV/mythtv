include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythexiv2-0.28
target.path = $${LIBDIR}
INSTALLS = target

darwin {
    QMAKE_CXXFLAGS = "-I. -I./include -I./include/exiv2 -I./src -I./xmpsdk/include" $${QMAKE_CXXFLAGS}
    LIBS += -lexpat -liconv -lz
} else {
    INCLUDEPATH += . ./include ./include/exiv2 ./src ./xmpsdk/include
    LIBS += -lexpat
}

contains(CC, gcc) {
    QMAKE_CXXFLAGS += -Wno-deprecated-copy
}
QMAKE_CXXFLAGS += -Wno-missing-declarations -Wno-shadow
QMAKE_CXXFLAGS += -Wno-zero-as-null-pointer-constant -Wno-double-promotion
QMAKE_CXXFLAGS += -Wno-undef

DEFINES += exiv2lib_EXPORTS

QMAKE_CLEAN += $(TARGET)

HEADERS += include/exiv2/basicio.hpp
HEADERS += include/exiv2/bigtiffimage.hpp
HEADERS += include/exiv2/bmpimage.hpp
HEADERS += include/exiv2/config.h
HEADERS += include/exiv2/convert.hpp
HEADERS += include/exiv2/cr2image.hpp
HEADERS += include/exiv2/crwimage.hpp
HEADERS += include/exiv2/datasets.hpp
HEADERS += include/exiv2/easyaccess.hpp
HEADERS += include/exiv2/error.hpp
HEADERS += include/exiv2/exif.hpp
HEADERS += include/exiv2/exiv2.hpp
HEADERS += include/exiv2/futils.hpp
HEADERS += include/exiv2/gifimage.hpp
HEADERS += include/exiv2/http.hpp
HEADERS += include/exiv2/image.hpp
HEADERS += include/exiv2/image_types.hpp
HEADERS += include/exiv2/ini.hpp
HEADERS += include/exiv2/iptc.hpp
HEADERS += include/exiv2/jp2image.hpp
HEADERS += include/exiv2/jpgimage.hpp
HEADERS += include/exiv2/metadatum.hpp
HEADERS += include/exiv2/mrwimage.hpp
HEADERS += include/exiv2/orfimage.hpp
HEADERS += include/exiv2/pgfimage.hpp
HEADERS += include/exiv2/pngimage.hpp
HEADERS += include/exiv2/preview.hpp
HEADERS += include/exiv2/properties.hpp
HEADERS += include/exiv2/psdimage.hpp
HEADERS += include/exiv2/rafimage.hpp
HEADERS += include/exiv2/rw2image.hpp
HEADERS += include/exiv2/slice.hpp
HEADERS += include/exiv2/tags.hpp
HEADERS += include/exiv2/tgaimage.hpp
HEADERS += include/exiv2/tiffimage.hpp
HEADERS += include/exiv2/types.hpp
HEADERS += include/exiv2/value.hpp
HEADERS += include/exiv2/version.hpp
HEADERS += include/exiv2/webpimage.hpp
HEADERS += include/exiv2/xmp_exiv2.hpp
HEADERS += include/exiv2/xmpsidecar.hpp

SOURCES += src/FileIo.cpp
SOURCES += src/MemIo.cpp
SOURCES += src/RemoteIo.cpp
SOURCES += src/actions.cpp
SOURCES += src/basicio.cpp
SOURCES += src/bigtiffimage.cpp
SOURCES += src/bmpimage.cpp
SOURCES += src/canonmn_int.cpp
SOURCES += src/casiomn_int.cpp
SOURCES += src/convert.cpp
SOURCES += src/cr2header_int.cpp
SOURCES += src/cr2image.cpp
SOURCES += src/crwimage.cpp
SOURCES += src/crwimage_int.cpp
SOURCES += src/datasets.cpp
SOURCES += src/easyaccess.cpp
SOURCES += src/error.cpp
SOURCES += src/exif.cpp
SOURCES += src/exiv2.cpp
SOURCES += src/fujimn_int.cpp
SOURCES += src/futils.cpp
SOURCES += src/getopt.cpp
SOURCES += src/gifimage.cpp
SOURCES += src/helper_functions.cpp
SOURCES += src/http.cpp
SOURCES += src/image.cpp
SOURCES += src/image_int.cpp
SOURCES += src/ini.cpp
SOURCES += src/iptc.cpp
SOURCES += src/jp2image.cpp
SOURCES += src/jpgimage.cpp
SOURCES += src/makernote_int.cpp
SOURCES += src/metadatum.cpp
SOURCES += src/minoltamn_int.cpp
SOURCES += src/mrwimage.cpp
SOURCES += src/nikonmn_int.cpp
SOURCES += src/olympusmn_int.cpp
SOURCES += src/orfimage.cpp
SOURCES += src/orfimage_int.cpp
SOURCES += src/panasonicmn_int.cpp
SOURCES += src/params.cpp
SOURCES += src/pentaxmn_int.cpp
SOURCES += src/pgfimage.cpp
SOURCES += src/pngchunk_int.cpp
SOURCES += src/pngimage.cpp
SOURCES += src/preview.cpp
SOURCES += src/properties.cpp
SOURCES += src/psdimage.cpp
SOURCES += src/rafimage.cpp
SOURCES += src/rw2image.cpp
SOURCES += src/rw2image_int.cpp
SOURCES += src/samsungmn_int.cpp
SOURCES += src/sigmamn_int.cpp
SOURCES += src/sonymn_int.cpp
SOURCES += src/tags.cpp
SOURCES += src/tags_int.cpp
SOURCES += src/tgaimage.cpp
SOURCES += src/tiffcomposite_int.cpp
SOURCES += src/tiffimage.cpp
SOURCES += src/tiffimage_int.cpp
SOURCES += src/tiffvisitor_int.cpp
SOURCES += src/types.cpp
SOURCES += src/utils.cpp
SOURCES += src/value.cpp
SOURCES += src/version.cpp
SOURCES += src/webpimage.cpp
SOURCES += src/xmp.cpp
SOURCES += src/xmpsidecar.cpp


SOURCES += xmpsdk/src/ExpatAdapter.cpp
SOURCES += xmpsdk/src/MD5.cpp
SOURCES += xmpsdk/src/ParseRDF.cpp
SOURCES += xmpsdk/src/UnicodeConversions.cpp
SOURCES += xmpsdk/src/UnicodeInlines.incl_cpp
SOURCES += xmpsdk/src/WXMPIterator.cpp
SOURCES += xmpsdk/src/WXMPMeta.cpp
SOURCES += xmpsdk/src/WXMPUtils.cpp
SOURCES += xmpsdk/src/XML_Node.cpp
SOURCES += xmpsdk/src/XMPCore_Impl.cpp
SOURCES += xmpsdk/src/XMPIterator.cpp
SOURCES += xmpsdk/src/XMPMeta-GetSet.cpp
SOURCES += xmpsdk/src/XMPMeta-Parse.cpp
SOURCES += xmpsdk/src/XMPMeta-Serialize.cpp
SOURCES += xmpsdk/src/XMPMeta.cpp
SOURCES += xmpsdk/src/XMPUtils-FileInfo.cpp
SOURCES += xmpsdk/src/XMPUtils.cpp

inc_exiv2.path = $${PREFIX}/include/mythtv/exiv2
inc_exiv2.files = $${HEADERS}

INSTALLS += inc_exiv2
