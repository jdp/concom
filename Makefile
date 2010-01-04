SRC = concom.c
OBJ = ${SRC:.c=.o}
CC = gcc
CFLAGS = -Wall
LIBS = -lreadline
OUT = concom

$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm $(OBJ) $(OUT)
	
.PHONY: clean

