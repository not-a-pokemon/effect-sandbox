.PHONY: default
default: efsa

CFLAGS+=-Wall -Wextra
# Use for testing only
# CFLAGS+=-ftrivial-auto-var-init=pattern
LDFLAGS!=pkg-config --libs sdl2 SDL2_ttf
SDL_CFLAGS!=pkg-config --cflags sdl2 SDL2_ttf

SRCS=\
	 main.c\
	 entity.c\
	 omalloc.c\
	 rng.c

OBJ=$(SRCS:.c=.o)

depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $(SRCS) >depend
include depend

util/structgen: util/structgen.c
	$(CC) $(CFLAGS) util/structgen.c -o util/structgen

gen-effects.h gen-loaders.h: util/structgen effect-data.txt
	./util/structgen .f gen-loaders.h .s gen-effects.h <effect-data.txt

efsa: $(OBJ)
	$(CC) $(OBJ) -o efsa $(LDFLAGS)

main.o:
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c -o main.o main.c

.PHONY: clean
clean:
	rm -f $(OBJ) efsa
