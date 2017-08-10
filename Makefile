CPPFLAGS := $(shell sdl2-config --cflags)
CFLAGS   += -Wall -Wpedantic -g
LDLIBS   := $(shell sdl2-config --libs) -lSDL2_image

all: prisma

clean:
	rm -fr prisma prisma.o
