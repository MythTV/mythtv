#################################################################################
#  
#  makefile
#
#################################################################################

CC       =  gcc
LINK     =  gcc
RM       =  rm 
RM_FLAGS =  -f

CFLAGS  	= -g -Wall -O
CDEFS   	= -DDEBUG

# Libraries

STDLIBS    = -lm
X11LIBS    =  -L/usr/X11R6/lib -lX11
PROGRAM    = mythtvosd

#################################################################################

.SUFFIXES: .o .c .flex
.c.o:   ;       $(CC)  $(CFLAGS) $(CDEFS) -c $< -o $*.o

mythtvosd: mythtvosd.o
	$(LINK) -o mythtvosd mythtvosd.o $(STDLIBS)

#################################################################################

clean:
	$(RM) $(RM_FLAGS) $(PROGRAM) *.o *~ core

#################################################################################
