TARGET=render.exe
LIBS=-lm -lX11
CC=gcc
CFLAGS=-Wall -O2

SOURCES=$(shell find . -name "*.c")
OBJECTS=$(SOURCES:.c=.o)
HEADERS=$(wildcard *.h)
WINDOW=1280x720
HIGH_RES=1920x1080
RESOLUTION=640x480
LOW_RES=320x200
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@ 

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(FLAGS) $(LIBS) -o $@
	-rm -f *.o

.PHONY: default all clean run run-high-res run-low-res

default: $(TARGET)
all: default
run: all
	./$(TARGET) -w $(WINDOW) -r $(RESOLUTION)

run-high-res: all
	./$(TARGET) -w $(WINDOW) -r $(HIGH_RES)

run-low-res: all
	./$(TARGET) -w $(WINDOW) -r $(LOW_RES)

clean:
	-rm -f *.o
	-rm -f $(TARGET)
