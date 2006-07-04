QMAKE_CLEAN += Makefile.perl

mythperlbindings.target = Makefile.perl
mythperlbindings.commands = perl Makefile.PL MAKEFILE=Makefile.perl
mythperlbindings.depends = Makefile.PL

mythperbindingsbuild.target = perl_build
mythperbindingsbuild.commands = @-make -f Makefile.perl
mythperbindingsbuild.depends = Makefile.perl

phony.target = .PHONY
phony.depends = perl_build perl_clean

perl_clean.target = perl_clean
perl_clean.commands = @-make -f Makefile.perl clean

qmake_clean.target = clean
qmake_clean.depends = perl_clean

perl_install.target = perl_install
perl_install.commands = @-make -f Makefile.perl install
perl_install.depends = perl_build

qmake_install.target = install
qmake_install.depends = perl_install

QMAKE_LINK=@-echo
PRE_TARGETDEPS += perl_build
QMAKE_EXTRA_UNIX_TARGETS += mythperlbindings mythperbindingsbuild phony perl_clean qmake_clean qmake_install perl_install

