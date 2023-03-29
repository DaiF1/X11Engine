CC 	= gcc
CFLAGS  =
INCLUDE = ./include
LIBS 	= -lX11 -lasound

OUT_DIR = ./build
EXEC 	= main

SRC 	= $(wildcard ./src/*.c)
OBJ 	= $(patsubst ./src/%.c,${OUT_DIR}/%.o,${SRC})

all: 	dev

setup:
	-mkdir ${OUT_DIR}

dev: 	CFLAGS += -Wall -Wextra -fsanitize=address -g -Og
dev: 	setup ${OBJ}
	${CC} ${CFLAGS} -o ${OUT_DIR}/${EXEC} ${OBJ} -I ${INCLUDE} ${LIBS}

release: CFLAGS += -O2
release: setup ${OBJ}
	${CC} ${CFLAGS} -o ${OUT_DIR}/${EXEC} ${OBJ} -I ${INCLUDE} ${LIBS}
	
${OUT_DIR}/%.o: ./src/%.c
	$(CC) $(CFLAGS) -c $^ -o $@ -I $(INCLUDE) $(LIBS)

clean:
	${RM} -r -d ${OUT_DIR}
