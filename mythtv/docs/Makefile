# To build this stuff, you need the linuxdoc tools.  On Debian, this
# means: linuxdoc-tools linuxdoc-tools-text
#  - mdz

TARGETS = mythtv-HOWTO.txt mythtv-HOWTO.html mythtv-HOWTO-singlehtml.html mythtv-HOWTO.pdf

all: $(TARGETS)
clean:
	rm -f $(TARGETS)

validate: mythtv-HOWTO.sgml
	linuxdoc -B check $<

%.txt: %.sgml
	linuxdoc -B txt $<

%.pdf: %.sgml
	linuxdoc -B latex -o pdf $<

%.html: %.sgml
	linuxdoc -B html --split=1 --toc=2 $<

%-singlehtml.html: %.sgml
	linuxdoc -B html --split=0 --toc=2 $<
