TARGET = red_eye
CFLAGS = -Wall
PREFIX = /usr/local
BINPATH = ${PREFIX}/bin

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

install: $(TARGET)
	install -m 0755 $(TARGET) ${BINPATH}

uninstall:
	rm -f ${BINPATH}/$(TARGET)

clean:
	rm -f $(TARGET)

distclean: clean
