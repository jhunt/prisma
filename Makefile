CPPFLAGS := $(shell sdl2-config --cflags)
CFLAGS   += -Wall -Wpedantic -g
LDFLAGS  := $(shell sdl2-config --libs)
LDLIBS   := -lSDL2_image

all: prisma

clean:
	rm -fr prisma prisma.o
