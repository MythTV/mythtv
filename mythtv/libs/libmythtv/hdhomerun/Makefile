
HDHOMERUN_CONFIG_SRCS += hdhomerun_pkt.c
HDHOMERUN_CONFIG_SRCS += hdhomerun_discover.c
HDHOMERUN_CONFIG_SRCS += hdhomerun_control.c
HDHOMERUN_CONFIG_SRCS += hdhomerun_config.c

CFLAGS += -Wall -O2

hdhomerun_config : $(HDHOMERUN_CONFIG_SRCS)
	gcc $(CFLAGS) $(HDHOMERUN_CONFIG_SRCS) -o hdhomerun_config
	strip hdhomerun_config

clean :
	rm -f hdhomerun_config
