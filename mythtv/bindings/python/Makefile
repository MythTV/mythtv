include ../../config.mak

ifdef INSTALL_ROOT
	ROOT_FLAGS = --root="$(INSTALL_ROOT)"
else
	ROOT_FLAGS = --root="/"
endif

REAL_PREFIX = $(PREFIX)
ifneq ($(PREFIX:/=), /usr)
	ifneq ($(PREFIX:/=), /usr/local)
		PREFIX_FLAGS=--prefix="$(PREFIX)"
	endif
endif

all: python_build

clean:
	$(PYTHON) setup.py clean --all

distclean: clean

python_build: setup.py
	$(PYTHON) setup.py build --mythtv-prefix="$(REAL_PREFIX)"

install: setup.py
	$(PYTHON) setup.py install --skip-build $(ROOT_FLAGS) $(PREFIX_FLAGS)

uninstall:
	$(warning make -C bindings/python uninstall is not supported for python bindings)

.PHONY: all clean distclean install python_build uninstall
