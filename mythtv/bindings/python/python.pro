include ( ../../config.mak )

python_build.target = python_build
python_build.depends = setup.py FORCE
python_build.commands = $$PYTHON setup.py build

QMAKE_EXTRA_TARGETS += python_build
PRE_TARGETDEPS += python_build

python_install.target = python_install
python_install.CONFIG = no_path
python_install.commands = $$PYTHON setup.py install --skip-build --root=\"$(if $(INSTALL_ROOT),$(INSTALL_ROOT),/)\"

INSTALLS += python_install

python_uninstall.target = python_uninstall
python_uninstall.depends = setup.py FORCE
python_uninstall.commands = $$PYTHON setup.py uninstall

QMAKE_EXTRA_TARGETS += python_uninstall

# This is done so the default /usr/local prefix installs to the "normal"
# /usr/lib/python{VER}/site-packages directory.
!contains(PREFIX, ^/usr(/local)?/?$) {
    python_install.commands += --prefix=\"$${PREFIX}\"
    python_uninstall.commands += --prefix=\"$${PREFIX}\"
}

uninstall.depends = python_uninstall
QMAKE_EXTRA_TARGETS += uninstall

QMAKE_CLEAN += dummy_file; $$PYTHON setup.py clean --all
QMAKE_LINK=@-echo

# Work around Qt 4.4 Mac bug:
macx : QMAKE_MACOSX_DEPLOYMENT_TARGET =
