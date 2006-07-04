QMAKE_CLEAN += Makefile.perl

mythperlbindings.target = Makefile.perl
mythperlbindings.depends = Makefile.PL
mythperlbindings.commands = perl Makefile.PL MAKEFILE=Makefile.perl

mythperbindingsbuild.target = perl_build
mythperbindingsbuild.depends = Makefile.perl
mythperbindingsbuild.commands = @-make -f Makefile.perl

phony.target = .PHONY
phony.depends = perl_build perl_clean

perl_clean.target = perl_clean
perl_clean.commands = @-make -f Makefile.perl clean

qmake_clean.target = clean
qmake_clean.depends = perl_clean

perl_install.target = install
perl_install.depends = all
perl_install.commands = make -f Makefile.perl pure_install PERL_INSTALL_ROOT="$(INSTALL_ROOT)"

QMAKE_LINK=@-echo
PRE_TARGETDEPS += perl_build
QMAKE_EXTRA_UNIX_TARGETS += mythperlbindings mythperbindingsbuild phony perl_clean qmake_clean perl_install

