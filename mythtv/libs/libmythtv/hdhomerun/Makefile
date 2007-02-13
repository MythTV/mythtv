
SRCS += hdhomerun_pkt.c
SRCS += hdhomerun_discover.c
SRCS += hdhomerun_control.c
SRCS += hdhomerun_config.c

CFLAGS += -Wall -O2

hdhomerun_config : $(SRCS)
	gcc $(CFLAGS) $(SRCS) -o $@
	strip $@

hdhomerun_config.exe : $(SRCS)
	gcc $(CFLAGS) $(SRCS) -liphlpapi -o $@
	strip $@

clean :
	rm -f hdhomerun_config
	rm -f hdhomerun_config.exe
