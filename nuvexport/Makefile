#
# Makefile for installing nuvexport
#

BINS	          = mpeg2cut nuvexport nuvinfo
MANS	          = man/nuvexport.1 \
                    man/nuvexport-svcd.1 \
                    man/nuvexport-vcd.1 \
                    man/nuvexport-xvid.1

EXPORT_MODULES	  = export/generic.pm         \
		    export/ffmpeg.pm          \
		    export/transcode.pm       \
		    export/MPEG2_cut.pm       \
		    export/NUV_SQL.pm         \
		    export/transcode/DVCD.pm  \
		    export/transcode/DVD.pm   \
		    export/transcode/SVCD.pm  \
		    export/transcode/VCD.pm   \
		    export/transcode/XviD.pm  \
		    export/ffmpeg/XviD.pm     \
		    export/ffmpeg/DivX.pm     \
		    export/ffmpeg/MP3.pm      \
		    export/ffmpeg/ASF.pm      \
		    export/ffmpeg/SVCD.pm     \
		    export/ffmpeg/DVCD.pm     \
		    export/ffmpeg/DVD.pm      \
		    export/ffmpeg/VCD.pm
MODULE_SUBDIRS    = transcode \
		    ffmpeg
MYTHTV_MODULES	  = mythtv/nuvinfo.pm \
		    mythtv/db.pm      \
		    mythtv/recordings.pm
NUVEXPORT_MODULES = nuv_export/help.pm         \
		    nuv_export/shared_utils.pm \
		    nuv_export/task.pm         \
		    nuv_export/ui.pm
MODULES		  = ${EXPORT_MODULES} ${MYTHTV_MODULES} ${NUVEXPORT_MODULES}

NUVEXPORT_LINKS   = divx dvcd dvd mp3 mpeg2cut nuvsql svcd vcd asf xvid

OWNER	= -o root -g root

INSTALL	= /usr/bin/install

prefix=/usr/local
bindir=${prefix}/bin
datadir=${prefix}/share
mandir=${prefix}/man

MODDIR=${datadir}/nuvexport
MAN1DIR=${mandir}/man1

default:
	@echo "Use \"make install\" to install the new version"


install:
	# First the binaries
	@for i in ${BINS} ; do \
	   ${INSTALL} -Dv ${OWNER} -m 0755 $$i ${bindir}/$$i; \
	done
	# Install the mode symlinks
	@for i in ${NUVEXPORT_LINKS} ; do \
	   ln -fs nuvexport ${bindir}/nuvexport-$$i; \
	done
	# Install the modules
	@for i in ${MODULES} ; do \
	   ${INSTALL} -Dv ${OWNER} -m 0755 $$i ${MODDIR}/$$i; \
	done
	# Install the man page
	@${INSTALL} -dv ${OWNER} -m 0755 ${MAN1DIR}
	@for i in ${MANS} ; do \
	   ${INSTALL} -v ${OWNER} -m 0644 $$i ${MAN1DIR}; \
	done


