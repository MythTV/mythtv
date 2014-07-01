
TEMPLATE = subdirs

using_live: LIBS += -lmythlivemedia-$$LIBVERSION
using_mheg: LIBS += -lmythfreemheg-$$LIBVERSION
LIBS += -lmythtv-$$LIBVERSION

# Directories
SUBDIRS = mytharchive mytharchivehelper theme i18n

