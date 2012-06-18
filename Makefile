INSTALL_DIR=/usr/local/bin
CFLAGS=-Wall -Werror -Wextra -pedantic -std=c99

ptydo:

install: ptydo
	cp $^ $(INSTALL_DIR)