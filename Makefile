#Makefile to build project - uses GCC or MinGW
INCLUDES :=  -I.
DEFINES :=

CFLAGS := $(INCLUDES) $(DEFINES) -g
CFLAGS += -Wall
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wmissing-prototypes

ifeq ($(OS),Windows_NT)
	TARGET := fcloc.exe
else
	TARGET := fcloc
endif

SRCS := fcloc.c

OBJS := ${SRCS:.c=.o}

all: ${TARGET}

${TARGET}: ${OBJS}
	${CC} -o $@ ${OBJS}

.c.o:
	${CC} -c ${CFLAGS} $*.c -o $@

depend:
	rm -f .depend
	${CC} -MM ${CFLAGS} *.c >> .depend

clean:
	rm -rf ${OBJS} ${TARGET}

run:
	./${TARGET}

include: .depend

.PHONY: all run clean
