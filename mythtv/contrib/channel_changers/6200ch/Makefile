TARGET=6200ch

all: $(TARGET)

$(TARGET): $(TARGET).c
	cc -std=gnu99 -o $(TARGET) $(TARGET).c -lavc1394 -lrom1394 -lraw1394

clean:
	rm -f $(TARGET)

distclean: clean
