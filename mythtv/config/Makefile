
all:	
qmake_all:

clean:
	-cd ../libs && qmake && make distclean
	-cd ../filters && qmake && make distclean
	-cd ../programs && qmake && make distclean
	-cd ../themes && qmake && make distclean
	-cd ../i18n && qmake && make distclean

install:
uninstall:

distclean: clean
	-cd ../setup && qmake && make distclean
	-cd ../docs && qmake && make distclean
	-rm -f ../libs/libmyth/mythconfig.mak
	-rm -f ../libs/libmyth/mythconfig.h
	-rm -f ../config.mak ../config.h
	-rm -f ../Makefile

