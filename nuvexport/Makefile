#
# Makefile for installing nuvexport
#

BINS	          = mpeg2cut nuvexport nuvinfo
MANS	          = man/nuvexport.1 \
                    man/nuvexport-svcd.1 \
                    man/nuvexport-vcd.1 \
                    man/nuvexport-xvid.1 
MODULE_DIRS	  = export mythtv nuv_export
EXPORT_MODULES	  = export/generic.pm   \
		    export/ffmpeg.pm    \
		    export/transcode.pm \
		    export/DVCD.pm      \
		    export/DVD.pm       \
		    export/MP3.pm       \
		    export/MPEG2_cut.pm \
		    export/NUV_SQL.pm   \
		    export/SVCD.pm      \
		    export/VCD.pm       \
		    export/WMV.pm       \
		    export/XviD.pm
MYTHTV_MODULES	  = mythtv/nuvinfo.pm \
		    mythtv/db.pm      \
		    mythtv/recordings.pm
NUVEXPORT_MODULES = nuv_export/shared_utils.pm \
		    nuv_export/task.pm         \
		    nuv_export/ui.pm
NUVEXPORT_LINKS   = dvcd dvd mp3 mpeg2cut nuvsql svcd vcd wmv xvid

OWNER	= -o root -g root

INSTALL	= /usr/bin/install

BINDIR	= /usr/local/bin
MANDIR	= /usr/local/man/man1
MODDIR	= /usr/local/share/nuvexport


default:
	@echo "Use \"make install\" to install the new version"


install:
	# First the binaries
	@for i in ${BINS} ; do \
	   ${INSTALL} -Dv ${OWNER} -m 0755 $$i ${BINDIR}/$$i; \
	done
	# Install the mode symlinks
	@for i in ${NUVEXPORT_LINKS} ; do \
	   ln -fs ${BINDIR}/nuvexport ${BINDIR}/nuvexport-$$i; \
	done
	# Install the modules
	@for i in ${EXPORT_MODULES} ${MYTHTV_MODULES} ${NUVEXPORT_MODULES} ; do \
	   ${INSTALL} -Dv ${OWNER} -m 0755 $$i ${MODDIR}/$$i; \
	done
	# Install the man page
	@${INSTALL} -dv ${OWNER} -m 0755 ${MANDIR}
	@for i in ${MANS} ; do \
	   ${INSTALL} -v ${OWNER} -m 0644 $$i ${MANDIR}; \
	done


