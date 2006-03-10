include ( ../../config.mak )
include ( ../../settings.pro )

INCLUDEPATH += ../libmythtv

TEMPLATE	= lib
TARGET = freemheg-$$LIBVERSION
CONFIG	+= thread dll
target.path = $${PREFIX}/lib
INSTALLS = target

SOURCES	+= Actions.cpp BaseClasses.cpp Bitmap.cpp Engine.cpp Groups.cpp Ingredients.cpp \
	ParseBinary.cpp ParseNode.cpp ParseText.cpp Presentable.cpp Programs.cpp Root.cpp \
	Stream.cpp Text.cpp Variables.cpp Visible.cpp BaseActions.cpp DynamicLineArt.cpp \
	TokenGroup.cpp Link.cpp
HEADERS	+= Actions.h ASN1Codes.h BaseClasses.h Bitmap.h Engine.h Groups.h Ingredients.h \
	ParseBinary.h ParseNode.h ParseText.h Presentable.h Programs.h Root.h \
	Stream.h Text.h Variables.h Visible.h BaseActions.h DynamicLineArt.h \
	TokenGroup.h Link.h Logging.h freemheg.h

LIBS += $$EXTRA_LIBS
