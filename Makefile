CPPFLAGS := $(shell sdl2-config --cflags)
CFLAGS   += -Wall -Wpedantic -g
LDLIBS   := $(shell sdl2-config --libs) -lSDL2_image

all: prisma joy

prisma: prisma.o map.o screen.o tiles.o util.o
joy: joy.o

clean:
	rm -fr prisma joy *.o
