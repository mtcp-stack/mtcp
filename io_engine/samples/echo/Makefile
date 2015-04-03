LIBS = -lps -lnuma
LIB_DIR = -L../../lib

.PHONY: clean

all: echo echo_new

echo: echo.c
	gcc -O3 -Wall -o echo echo.c $(LIB_DIR) $(LIBS)

echo_new: echo_new.c
	gcc -O3 -Wall -o echo_new echo_new.c $(LIB_DIR) $(LIBS)

clean:
	rm -f echo echo_new

