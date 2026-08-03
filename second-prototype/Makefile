# TARGET=render.exe
# LIBS=-lm -lX11
# CC=gcc
# CFLAGS=-Wall -O3
# 
# SOURCES=$(shell find . -name "*.c")
# OBJECTS=$(SOURCES:.c=.o)
# HEADERS=$(wildcard *.h)
# WINDOW=1280x720
# HIGH_RES=1920x1080
# RESOLUTION=640x480
# LOW_RES=320x200
# %.o: %.c $(HEADERS)
# 	$(CC) $(CFLAGS) -c $< -o $@ 
# 
# $(TARGET): $(OBJECTS)
# 	$(CC) $(OBJECTS) $(FLAGS) $(LIBS) -o $@
# 	-rm -f *.o
# 
# .PHONY: default all clean run run-high-res run-low-res
# 
# default: $(TARGET)
# all: default
# 
# run: all
# 	./$(TARGET) -w $(WINDOW) -r $(LOW_RES)
# 
# run-max-res: all
# 	./$(TARGET) -w $(WINDOW) -r $(HIGH_RES)
# 
# run-high-res: all
# 	./$(TARGET) -w $(WINDOW) -r $(RESOLUTION)
# 
# clean:
# 	-rm -f *.o
# 	-rm -f $(TARGET)


TARGET=render.exe

LIBS=-lm -lX11
CC=gcc

CFLAGS=-Wall -O3

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
	$(CC) $(OBJECTS) $(LIBS) -o $@


.PHONY: default all clean run run-high-res run-max-res debug gdb perf sanitize


default: $(TARGET)
all: default


run: all
	./$(TARGET) -w $(WINDOW) -r $(LOW_RES)

run-max-res: all
	./$(TARGET) -w $(WINDOW) -r $(HIGH_RES)

run-high-res: all
	./$(TARGET) -w $(WINDOW) -r $(RESOLUTION)

debug: CFLAGS=-Wall -g -O0
debug: clean $(TARGET)

gdb: debug
	gdb --args ./$(TARGET) -w $(WINDOW) -r $(LOW_RES)

perf: CFLAGS=-Wall -g -O2
perf: clean $(TARGET)
	perf record -g ./$(TARGET) -w $(WINDOW) -r $(RESOLUTION)
	perf report
	rm perf.*

clean:
	-rm -f *.o
	-rm -f $(TARGET)
