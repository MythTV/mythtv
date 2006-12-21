
SRCS += hdhomerun_pkt.c
SRCS += hdhomerun_discover.c
SRCS += hdhomerun_control.c
SRCS += hdhomerun_video.c
SRCS += hdhomerun_device.c
SRCS += hdhomerun_config.c

CFLAGS += -Wall -O2 -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith

hdhomerun_config : $(SRCS)
	gcc $(CFLAGS) $(SRCS) -lpthread -o $@
	strip $@

hdhomerun_config.exe : $(SRCS)
	gcc $(CFLAGS) $(SRCS) -lpthread -liphlpapi -o $@
	strip $@

clean :
	rm -f hdhomerun_config
	rm -f hdhomerun_config.exe
