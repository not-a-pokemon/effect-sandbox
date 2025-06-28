.PHONY:
default: efsa

CFLAGS+=-Wall -Wextra
LDFLAGS+=-lSDL2 -lSDL2_ttf

SRCS=$(wildcard *.c)
depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $(SRCS) >depend
include depend

OBJ=main.o entity.o omalloc.o rng.o
efsa: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o efsa

clean:
	rm -f $(OBJ) efsa
