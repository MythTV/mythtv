include ( ../../config.mak )

python_install.target = install
python_install.depends = all

contains(PREFIX, ^/usr(/local)?/?$) {
python_install.commands = python setup.py install --root="$(INSTALL_ROOT)"
} else {
python_install.commands = python setup.py install --root="$(INSTALL_ROOT)" --prefix="$${PREFIX}"
}

QMAKE_LINK=@-echo
QMAKE_EXTRA_UNIX_TARGETS += python_install

