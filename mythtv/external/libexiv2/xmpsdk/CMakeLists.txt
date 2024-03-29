add_library(exiv2-xmp STATIC
    src/ExpatAdapter.cpp
    src/MD5.cpp
    src/ParseRDF.cpp
    src/UnicodeConversions.cpp
    src/WXMPIterator.cpp
    src/WXMPMeta.cpp
    src/WXMPUtils.cpp
    src/XML_Node.cpp
    src/XMPCore_Impl.cpp
    src/XMPIterator.cpp
    src/XMPMeta-GetSet.cpp
    src/XMPMeta-Parse.cpp
    src/XMPMeta-Serialize.cpp
    src/XMPMeta.cpp
    src/XMPUtils-FileInfo.cpp
    src/XMPUtils.cpp
    include/MD5.h
    include/TXMPIterator.hpp
    include/TXMPMeta.hpp
    include/TXMPUtils.hpp
    include/XMP_Const.h
    include/XMP_Environment.h
    include/XMP.incl_cpp
    include/XMPSDK.hpp
    include/XMP_Version.h
)

# TODO: Use imported target EXPAT::EXPAT when we upgrade CMake version
target_link_libraries(exiv2-xmp
    PRIVATE 
        ${EXPAT_LIBRARY}
)

target_include_directories(exiv2-xmp
    PUBLIC 
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/xmpsdk/include>
    PRIVATE 
        ${EXPAT_INCLUDE_DIR}
)

# Prevent a denial-service-attack related to XML entity expansion
# ("billion laughs attack").
# See https://bugzilla.redhat.com/show_bug.cgi?id=888769
target_compile_definitions(exiv2-xmp PRIVATE BanAllEntityUsage=1)
if (MSVC)
    target_compile_definitions(exiv2-xmp PRIVATE XML_STATIC)
endif()

check_include_file( "stdint.h"  EXV_HAVE_STDINT_H )
if (EXV_HAVE_STDINT_H)
    target_compile_definitions(exiv2-xmp PRIVATE EXV_HAVE_STDINT_H)
endif()

if (BUILD_SHARED_LIBS)
    set_property(TARGET exiv2-xmp PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

set_target_properties( exiv2-xmp PROPERTIES
    OUTPUT_NAME   mythexiv2-xmp)

# 1119  Install libxmp.a for use by third party applications (Thanks, Emmanuel)
install(TARGETS exiv2-xmp EXPORT mythexiv2Config
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
