CPPFLAGS := $(shell sdl2-config --cflags)
CFLAGS   += -Wall -Wpedantic -g
LDLIBS   := $(shell sdl2-config --libs) -lSDL2_image

all: prisma joy

prisma: prisma.o map.o sprite.o tiles.o util.o world.o
joy: joy.o

clean:
	rm -fr prisma joy *.o *.dSYM/
