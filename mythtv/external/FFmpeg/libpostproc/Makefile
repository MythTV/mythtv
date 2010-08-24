include $(SUBDIR)../config.mak

NAME = postproc
FFLIBS = avcodec avcore avutil

# x86_32 needs -O1 -fomit-frame-pointer to compile inline asm
ifeq ($(ARCH_X86_32), yes)
    CFLAGS += -fomit-frame-pointer
    ifneq (, $(findstring debug, $(CCONFIG)))
        CFLAGS += -O1
    endif
endif

HEADERS = postprocess.h

OBJS = postprocess.o

include $(SUBDIR)../subdir.mak
