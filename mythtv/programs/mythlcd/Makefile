####### Compiler, tools and options

CXX      = g++
#
# debug:
#

CXXFLAGS = -Wall -DLCD_DEVICE -DLCD_DEVICE_DEBUG -I$(QTDIR)/include		

#
# no debug:
#

#CXXFLAGS = -Wall -DLCD_DEVICE -I$(QTDIR)/include		


LINK	 = -Wl,-rpath,$(QTDIR)/lib  -L$(QTDIR)/lib  -L/usr/X11R6/lib  -L/usr/X11R6/lib -lqt-mt -lpthread -lXmu -lICE -lXext -lX11 -lm
MOC	 = $(QTDIR)/bin/moc

all: 			main.o lcddevice.o moc_lcddevice.o mythlcd.o moc_mythlcd.o
			$(CXX) lcddevice.o moc_lcddevice.o main.o mythlcd.o moc_mythlcd.o -o mythlcd $(LINK)

main.o:			main.cpp lcddevice.h 
			$(CXX) $(CXXFLAGS) main.cpp -c

lcddevice.o:		lcddevice.h lcddevice.cpp
			$(CXX) $(CXXFLAGS) lcddevice.cpp -c

moc_lcddevice.o:	lcddevice.h lcddevice.cpp
			$(MOC) lcddevice.h -o moc_lcddevice.cpp
			$(CXX) $(CXXFLAGS) moc_lcddevice.cpp -c

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
