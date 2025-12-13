CC     = gcc
CFLAGS = -lX11 -lXpm -lXft -lfontconfig -I/usr/include/freetype2 

sillywm: wm.c config.h
	@$(CC) $< -o $@ $(CFLAGS) 

config.h: config.def.h
	@cp $< $@
