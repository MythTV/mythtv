#ifndef MYTHVERSION_H_
#define MYTHVERSION_H_

#include "qglobal.h"
#include "mythexp.h"

#if ( QT_VERSION < 0x040400 )
#error You need Qt version >= 4.4.0 to compile MythTV.
#endif

/// Update this whenever the plug-in API changes.
/// Including changes in the libmythdb, libmyth, libmythtv, libmythav* and
/// libmythui class methods used by plug-ins.
#define MYTH_BINARY_VERSION "0.23.201000617-1"

/** \brief Increment this whenever the MythTV network protocol changes.
 *
 *   You must also update this value and any corresponding changes to the
 *   ProgramInfo network protocol layout in the following files:
 *
 *   MythWeb
 *       mythplugins/mythweb/classes/MythBackend.php (version number)
 *       mythplugins/mythweb/modules/tv/includes/objects/Program.php (layout)
 *
 *   MythTV Perl Bindings
 *       mythtv/bindings/perl/MythTV.pm (version number)
 *       mythtv/bindings/perl/MythTV/Program.pm (layout)
 *
 *   MythTV Python Bindings
 *       mythtv/bindings/python/MythTV/MythTV.py (version number)
 *       mythtv/bindings/python/MythTV/MythTV.py (layout)
 */
#define MYTH_PROTO_VERSION "56"

MPUBLIC const char *GetMythSourceVersion();

#endif

