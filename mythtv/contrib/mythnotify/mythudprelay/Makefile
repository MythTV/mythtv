#################################################################################
#  
#  makefile
#
#################################################################################

CC       =  gcc
LINK     =  gcc
RM       =  rm 
RM_FLAGS =  -f

CFLAGS  	= -g -Wall `xml2-config --cflags` -O
CDEFS   	= -DDEBUG

# Libraries

STDLIBS    = -lm `xml2-config --libs` -lxslt
X11LIBS    =  -L/usr/X11R6/lib -lX11
PROGRAM = mythudprelay

#################################################################################

.SUFFIXES: .o .c .flex
.c.o:   ;       $(CC)  $(CFLAGS) $(CDEFS) -c $< -o $*.o

mythudprelay: mythudprelay.o
	$(LINK) -o mythudprelay mythudprelay.o $(STDLIBS)

#################################################################################

clean:
	$(RM) $(RM_FLAGS) $(PROGRAM) *.o *~ core

#################################################################################
