####### Compiler, tools and options

CXX      = g++
#
# debug:
#

CXXFLAGS = -Wall -DLCD_DEVICE -DLCD_DEVICE_DEBUG -I$(QTDIR)/include -I../../libs/libmyth

#
# no debug:
#

#CXXFLAGS = -Wall -DLCD_DEVICE -I$(QTDIR)/include -I../../libs/libmyth


LINK	 = -Wl,-rpath,$(QTDIR)/lib  -L../../libs/libmyth -L../../libs/libmythtv -L$(QTDIR)/lib  -L/usr/X11R6/lib  -L/usr/X11R6/lib -lqt-mt -lpthread -lXmu -lICE -lXext -lX11 -lm -lmyth
MOC	 = $(QTDIR)/bin/moc

all: 			main.o mythlcd.o moc_mythlcd.o
			$(CXX) main.o mythlcd.o moc_mythlcd.o -o mythlcd $(LINK)

main.o:			main.cpp 
			$(CXX) $(CXXFLAGS) main.cpp -c

mythlcd.o:		mythlcd.h mythlcd.cpp
			$(CXX) $(CXXFLAGS) mythlcd.cpp -c

moc_mythlcd.o:		mythlcd.h mythlcd.cpp
			$(MOC) mythlcd.h -o moc_mythlcd.cpp
			$(CXX) $(CXXFLAGS) moc_mythlcd.cpp -c

clean:
			rm -f *~
			rm -f *.o
			rm -f moc_*
			rm -f mythlcd
