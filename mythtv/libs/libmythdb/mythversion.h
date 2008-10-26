#ifndef MYTHVERSION_H_
#define MYTHVERSION_H_

#if (QT_VERSION < 0x040300)
#error You need Qt version >= 4.3.0 to compile MythTV.
#endif

/// Update this whenever the plug-in API changes.
/// Including changes in the libmyth, libmythtv and libmythui class methods
/// used by plug-ins.
#define MYTH_BINARY_VERSION "0.22.20081026-1"

/** \brief Increment this whenever the MythTV network protocol changes.
 *
 *   You must also update this value and any corresponding changes to the
 *   ProgramInfo network protocol layout in the following files:
 *
 *   MythWeb
 *       mythplugins/mythweb/includes/mythbackend.php (version number)
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
#define MYTH_PROTO_VERSION "42"

#endif

