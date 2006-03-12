include ( ../../config.mak )
include ( ../../settings.pro )

INCLUDEPATH += ../libmythtv

TEMPLATE = lib
TARGET = freemheg-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

SOURCES	+= Actions.cpp BaseClasses.cpp Bitmap.cpp Engine.cpp Groups.cpp 
SOURCES += Ingredients.cpp ParseBinary.cpp ParseNode.cpp ParseText.cpp 
SOURCES += Presentable.cpp Programs.cpp Root.cpp Stream.cpp Text.cpp 
SOURCES += Variables.cpp Visible.cpp BaseActions.cpp DynamicLineArt.cpp 
SOURCES += TokenGroup.cpp Link.cpp

HEADERS	+= Actions.h ASN1Codes.h BaseClasses.h Bitmap.h Engine.h Groups.h 
HEADERS += Ingredients.h ParseBinary.h ParseNode.h ParseText.h Presentable.h 
HEADERS += Programs.h Root.h Stream.h Text.h Variables.h Visible.h BaseActions.h
HEADERS += DynamicLineArt.h TokenGroup.h Link.h Logging.h freemheg.h

