PROGNAME = imgcmp
rm = /bin/rm -f
CC = cc
CXX = c++
AS = as
DEFS = -Wno-multichar
INCLUDES = -I. -I/usr/local/include
LIBS = -L/usr/local/lib -lpthread -lgd
DEFINES = $(INCLUDES) $(DEFS) -DSYS_UNIX=1

CFLAGS = -pipe -Wall -O3 $(DEFINES) -march=native
CXXFLAGS = -pipe -Wall -O3 $(DEFINES)
ASFLAGS = 

SOURCES = bptree.c \
dedup.c \
hashtable.c \
img.c \
main.c \
mmfile.c \
test.c \
thumb.c \
vector.c

OBJECTS = ${SOURCES:.c=.o}
SRCS = ${addprefix src/,$(SOURCES)}
OBJS = ${addprefix obj/,$(OBJECTS)}

.SILENT:

obj/%.o: src/%.c
	mkdir -p $(@D);
	#$(rm) $@;
	if ${CC} ${CFLAGS} -c -o $@ $<; then \
		printf "\033[32mbuilt $@.\033[m\n"; \
	else \
		printf "\033[31mbuild of $@ failed!\033[m\n"; \
		false; \
	fi
	
obj/%.o: src/%.cpp
	mkdir -p $(@D);
	$(rm) $@;
	if ${CXX} ${CXXFLAGS} -c -o $@ $<; then \
		printf "\033[32mbuilt $@.\033[m\n"; \
	else \
		printf "\033[31mbuild of $@ failed!\033[m\n"; \
		false; \
	fi
	
obj/%.o: src/%.s
	mkdir -p $(@D);
	$(rm) $@;
	if ${AS} ${ASFLAGS} $< -o $@; then \
		printf "\033[32mbuilt $@.\033[m\n"; \
	else \
		printf "\033[31mbuild of $@ failed!\033[m\n"; \
		false; \
	fi
	
all: $(PROGNAME)

debug: CFLAGS = -pipe -Wall -march=i686 -g $(DEFINES)
debug: $(PROGNAME)

$(PROGNAME) : $(OBJS)
	if $(CC) $(CFLAGS) -o $(PROGNAME) $(OBJS) $(LIBS); then \
		printf "\033[32mlinked $@.\033[m\n"; \
	else \
		printf "\033[31mlink of $@ failed!\033[m\n"; \
		false; \
	fi

clean:
	$(rm) $(PROGNAME) core *~
	$(rm) -rf obj/*
