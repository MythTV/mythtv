TARGETS = devdocs

DOXYGEN_SOURCES=doxygen-architecture-document.cpp ../contrib ../libs ../programs ../../mythplugins
DOXYGEN_EXCLUDES=../libs/libmythmpeg2

all: $(TARGETS)
clean:
	rm -Rf doxygen-paths.cfg.inc doxygen-dev-docs doxygen-warnings.txt

distclean: clean

doxygen-paths.cfg.inc: doxygen-paths.cfg.inc.in
	@echo Creating doxygen-paths.cfg.inc
	@sed -e "s#@DOXYGEN_SOURCES@#${DOXYGEN_SOURCES}#g" \
	    -e "s#@DOXYGEN_EXCLUDES@#${DOXYGEN_EXCLUDES}#g" \
	    doxygen-paths.cfg.inc.in > doxygen-paths.cfg.inc

devdocs: doxygen-paths.cfg.inc
	doxygen doxygen-create-developer-docs.cfg
