TARGETS = devdocs

all: $(TARGETS)
clean:
	rm -Rf doxygen-dev-docs doxygen-warnings.txt

distclean: clean

devdocs:
	doxygen doxygen-create-developer-docs.cfg
