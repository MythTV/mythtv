include ( ../../config.mak )

QMAKE_CLEAN += filethatdoesntexist ; if [ -f Makefile.perl ] ; then $(MAKE) -f Makefile.perl clean; rm -f Makefile.perl ; fi

mythperlbindings.target = Makefile.perl
mythperlbindings.depends = Makefile.PL
mythperlbindings.commands = perl Makefile.PL MAKEFILE=Makefile.perl PREFIX=$${PREFIX} $${PERL_CONFIG_OPTS}
mythperbindingsbuild.target = perl_build
mythperbindingsbuild.depends = Makefile.perl
mythperbindingsbuild.commands = @-$(MAKE) -f Makefile.perl

phony.target = .PHONY
phony.depends = perl_build

perl_install.target = install
perl_install.depends = all
perl_install.commands = $(MAKE) -f Makefile.perl pure_install PERL_INSTALL_ROOT=\"$(INSTALL_ROOT)\" PREFIX=$${PREFIX}

QMAKE_LINK=@-echo
PRE_TARGETDEPS += perl_build
QMAKE_EXTRA_UNIX_TARGETS += mythperlbindings mythperbindingsbuild phony perl_clean qmake_clean perl_install

