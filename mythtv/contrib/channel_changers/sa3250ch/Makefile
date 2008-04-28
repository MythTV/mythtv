TARGET=sa3250ch
CFLAGS = -Wall -std=gnu99 
PREFIX = /usr/local
BINPATH = ${PREFIX}/bin

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c -lavc1394 -lrom1394 -lraw1394

install: $(TARGET)
	install -m 0755 $(TARGET) ${BINPATH}

uninstall:
	rm -f ${BINPATH}/$(TARGET)

clean:
	rm -f $(TARGET)

distclean: clean
