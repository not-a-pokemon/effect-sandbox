.PHONY:
default: efsa

CFLAGS+=-Wall -Wextra
LDFLAGS+=-lSDL2 -lSDL2_ttf

SRCS=$(wildcard *.c)
depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $(SRCS) >depend
include depend

util/structgen: util/structgen.c
	$(CC) $(CFLAGS) util/structgen.c -o util/structgen

gen-effects.h gen-loaders.h: util/structgen effect-data.txt
	./util/structgen .f gen-loaders.h .s gen-effects.h <effect-data.txt

OBJ=main.o entity.o omalloc.o rng.o
efsa: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o efsa

clean:
	rm -f $(OBJ) efsa
