INSTALL_DIR=/usr/local/bin
CFLAGS=-Wall -Werror -Wextra -pedantic -std=c99 -O3

ptydo:

clean:
	rm -f ptydo

install: ptydo
	cp $^ $(INSTALL_DIR)