TARGET=render.exe
LIBS=-lm -lX11
CC=gcc
CFLAGS=-Wall

SOURCES=$(shell find . -name "*.c")
OBJECTS=$(SOURCES:.c=.o)
HEADERS=$(wildcard *.h)
WINDOW=1280x720
RESOLUTION=800x640
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@ 

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(FLAGS) $(LIBS) -o $@
	-rm -f *.o

.PHONY: default all clean run

default: $(TARGET)
all: default
run: all
	./$(TARGET) -w $(WINDOW) -r $(RESOLUTION)

clean:
	-rm -f *.o
	-rm -f $(TARGET)
