# To build this stuff, you need the linuxdoc tools.  On Debian, this
# means: linuxdoc-tools linuxdoc-tools-text
#  - mdz

TARGETS = mythtv-HOWTO.txt mythtv-HOWTO.html mythtv-HOWTO-singlehtml.html mythtv-HOWTO.pdf
LINUXDOC_HTML_SPLIT := 1

all: $(TARGETS)
clean:
	rm -f $(TARGETS) $(patsubst %.html,%-[0-9]*.html,$(filter %.html,$(TARGETS)))
distclean: clean

validate: mythtv-HOWTO.sgml
	linuxdoc -B check $<

%.txt: %.sgml
	linuxdoc -B txt $<

%.pdf: %.sgml
	linuxdoc -B latex -o pdf $<

%.html: %.sgml
	linuxdoc -B html --split=$(LINUXDOC_HTML_SPLIT) --toc=2 $<

%-singlehtml.html: LINUXDOC_HTML_SPLIT=0
%-singlehtml.sgml: %.sgml
	cp $< $@
