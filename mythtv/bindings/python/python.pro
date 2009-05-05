include ( ../../config.mak )

python_build.target = python_build
python_build.depends = setup.py FORCE
python_build.commands = python setup.py build

python_install.target = python_install
python_install.CONFIG = no_path
python_install.commands = python setup.py install --skip-build --root=\"$(if $(INSTALL_ROOT),$(INSTALL_ROOT),/)\"

# This is done so the default /usr/local prefix installs to the "normal"
# /usr/lib/python{VER}/site-packages directory.
!contains(PREFIX, ^/usr(/local)?/?$) {
	python_install.commands += --prefix=\"$${PREFIX}\"
}

QMAKE_CLEAN += dummy_file; python setup.py clean --all

PRE_TARGETDEPS += python_build
QMAKE_LINK=@-echo
QMAKE_EXTRA_UNIX_TARGETS += python_build
INSTALLS += python_install

# Work around Qt 4.4 Mac bug:
macx : QMAKE_MACOSX_DEPLOYMENT_TARGET =
