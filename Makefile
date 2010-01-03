SRC = concom.c
OBJ = ${SRC:.c=.o}
CC = gcc
CFLAGS = -Wall
OUT = concom

$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm $(OBJ) $(OUT)
	
.PHONY: clean

