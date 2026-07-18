TARGET=render.exe
LIBS=-lm -lX11
CC=gcc
CFLAGS=-Wall

SOURCES=$(shell find . -name "*.c")
OBJECTS=$(SOURCES:.c=.o)
HEADERS=$(wildcard *.h)
WIN_H=640
WIN_W=800

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@ 

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(FLAGS) $(LIBS) -o $@
	-rm -f *.o

.PHONY: default all clean run

default: $(TARGET)
all: default
run: all
	./$(TARGET) $(WIN_W) $(WIN_H) 

clean:
	-rm -f *.o
	-rm -f $(TARGET)
