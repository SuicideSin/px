CC      := clang
CFLAGS  := -Wall -pedantic -std=c99 -O0 -g
LDFLAGS := -lGLU -lGL -lm -lglfw
INCS    := -I../
SRC     := $(wildcard *.c)
OBJ     := $(SRC:.c=.o)
TARGET  := px

all: $(TARGET)

%.o: %.c glyphs.h
	@echo "cc   $< => $@"
	@$(CC) -c $(CFLAGS) $(INCS) -o $@ $<

$(TARGET): $(OBJ)
	@echo "=>   $(TARGET)"
	@$(CC) $(OBJ) $(LDFLAGS) -o $(TARGET)
	@echo OK

%.d: %.c
	@echo "dep  $*.o => $*.d"
	@$(CC) $(INCS) -MM -MG -MT "$*.o $*.d" $*.c >$@

-include $(OBJ:.o=.d)

glyphs: glyphs.h

glyphs.h: glyphs.tga
	@echo "glyphs.tga => glyphs.h"
	@$(CC) -I./ glyphs/glyphs.c tga.c -o glyphs/glyphs
	@glyphs/glyphs > glyphs.h

clean:
	@rm glyphs.h glyphs/glyphs
	@rm -f $(OBJ) *.d
	@[ -f $(TARGET) ] && rm $(TARGET)

