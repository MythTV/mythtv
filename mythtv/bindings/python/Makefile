include ../../config.mak

ifdef INSTALL_ROOT
	ROOT_FLAGS = --root="$(INSTALL_ROOT)"
else
ifndef USE_PYTHON_PIP
	ROOT_FLAGS = --root="/"
endif
endif

ifneq ($(PREFIX:/=), /usr)
	ifneq ($(PREFIX:/=), /usr/local)
		PREFIX_FLAGS=--prefix="$(PREFIX)"
	endif
endif

PIP_OPTIONS = --no-build-isolation --no-cache-dir --no-index --disable-pip-version-check --no-deps
WHEEL_DIR = dist

all: python_build

ifdef USE_PYTHON_PIP

clean:
	$(RM) -r $(WHEEL_DIR)
	$(RM) -r build
	$(RM) -r MythTV.*

distcean: clean

python_build:
	$(PYTHON) -m pip wheel $(PIP_OPTIONS) --wheel-dir ./$(WHEEL_DIR) .

install:
	$(PYTHON) -m pip install $(ROOT_FLAGS) $(PREFIX_FLAGS) $(PIP_OPTIONS) --ignore-installed --find-links ./$(WHEEL_DIR) MythTV

uninstall:
	$(warning python pip uninstall is not supported for python bindings)

else

clean:
	$(PYTHON) setup.py clean --all

distclean: clean

python_build: setup.py
	$(PYTHON) setup.py build

install: setup.py
	$(PYTHON) setup.py install --skip-build $(ROOT_FLAGS) $(PREFIX_FLAGS)

uninstall:
	$(warning make -C bindings/python uninstall is not supported for python bindings)

endif

.PHONY: all clean distclean install python_build uninstall
