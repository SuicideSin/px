CC      := clang
CFLAGS  := -Wall -pedantic -std=c99 -O0 -g $(shell pkg-config --cflags glfw3)
LDFLAGS := $(shell pkg-config --static --libs glfw3)
INCS    := -I../
SRC     := $(wildcard *.c)
OBJ     := $(SRC:.c=.o)
TARGET  := px

all: glyphs $(TARGET)

%.o: %.c
	$(CC) -c $(CFLAGS) $(INCS) -o $@ $<

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(TARGET)

glyphs: glyphs.h

glyphs.h: glyphs.tga
	$(CC) -I./ glyphs/glyphs.c tga.c -o glyphs/glyphs
	glyphs/glyphs > glyphs.h

clean:
	rm -f glyphs.h glyphs/glyphs $(OBJ) $(TARGET)
