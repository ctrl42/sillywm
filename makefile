CC     = gcc
CFLAGS = -O2 -lX11 -lXpm -lXft -lfontconfig -I/usr/include/freetype2 
PREFIX = /usr

sillywm: wm.c config.h
	@$(CC) $< -o $@ $(CFLAGS) 

config.h: config.def.h
	@cp $< $@

install: sillywm
	@mkdir -p $(PREFIX)/bin
	@cp sillywm $(PREFIX)/bin/
