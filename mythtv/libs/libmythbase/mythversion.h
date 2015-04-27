#ifndef MYTHVERSION_H_
#define MYTHVERSION_H_

#include "qglobal.h"
#include "mythbaseexp.h"
#include "version.h"

#if ( QT_VERSION < 0x040800 )
#error You need Qt version >= 4.8.0 to compile MythTV.
#endif

/// Update this whenever the plug-in ABI changes.
/// Including changes in the libmythbase, libmyth, libmythtv, libmythav* and
/// libmythui class methods in exported headers.
#define MYTH_BINARY_VERSION "0.28.20150304-1"

/** \brief Increment this whenever the MythTV network protocol changes.
 *
 *   You must also update this value and any corresponding changes to the
 *   ProgramInfo network protocol layout in the following files:
 *
 *   MythWeb
 *       mythweb/modules/tv/classes/Program.php (layout)
 *
 *   MythTV Perl Bindings
 *       mythtv/bindings/perl/MythTV.pm (PROTO_VERSION, PROTO_TOKEN)
 *       mythtv/bindings/perl/MythTV.pm (NUMPROGRAMLINES)
 *       mythtv/bindings/perl/MythTV/Program.pm (_parse_data, to_string)
 *
 *   MythTV PHP Bindings
 *       mythtv/bindings/php/MythBackend.php (protocol_version, protocol_token)
 *       mythtv/bindings/php/MythBackend.php (program_line_number)
 *       mythtv/bindings/php/MythTVProgram.php (layout)
 *         (but only to reflect new columns in the program table)
 *       mythtv/bindings/php/MythTVRecording.php (layout)
 *         (but only to reflect new columns in the recorded table)
 *
 *   MythTV Python Bindings
 *       mythtv/bindings/python/MythTV/static.py (PROTO_VERSION, PROTO_TOKEN)
 *       mythtv/bindings/python/MythTV/mythproto.py (layout)
 *
 *   Be kind and update the wiki as well.
 *       http://www.mythtv.org/wiki/Category:Myth_Protocol_Commands
 *       http://www.mythtv.org/wiki/Category:Myth_Protocol
 */
#define MYTH_PROTO_VERSION "86"
#define MYTH_PROTO_TOKEN "(ノಠ益ಠ)ノ彡┻━┻"

/** \brief Increment this whenever the MythTV core database schema changes.
 *
 *  You must update the schema handler to implement the new schema:
 *      mythtv/libs/libmythtv/dbcheck.cpp
 *
 *  You must also update the following files to independently check:
 *
 *  MythTV Perl Bindings
 *      mythtv/bindings/perl/MythTV.pm
 *
 *  MythTV Python Bindings
 *      mythtv/bindings/python/MythTV/static.py
 */
#if 0
 *
 *  MythTV PHP Bindings
 *      mythtv/bindings/php/MythBackend.php
#endif

#define MYTH_DATABASE_VERSION "1339"


 MBASE_PUBLIC  const char *GetMythSourceVersion();

#endif
