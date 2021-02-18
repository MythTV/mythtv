// qt
#include <QString>

// mythtv
#include "mythmetaexp.h"

class MusicMetadata;

// These are needed to handle taglib < 1.10 which has obsolete Qt utf8
// calls.  Once all supported releases have taglib 1.10 these can be
// deleted.  (They're copied from taglib 1.10.)  Currently centos7,
// debian jessie, and Ubuntu 16.06 still have taglib 1.8 or 1.9.
#undef QStringToTString
#define QStringToTString(s) TagLib::String((s).toUtf8().data(), TagLib::String::UTF8)
#undef TStringToQString
#define TStringToQString(s) QString::fromUtf8((s).toCString(true))

/// remove any bad filename characters
META_PUBLIC QString fixFilename(const QString &filename);

/// remove any bad filename characters (leaving '/' untouched)
META_PUBLIC QString fixFileToken_sl (QString token);

/// find an image for a artist or genre
META_PUBLIC QString findIcon(const QString &type, const QString &name, bool ignoreCache = false);

/// create a filename using the template in the settings and a MusicMetadata object
META_PUBLIC QString filenameFromMetadata(MusicMetadata *track);

/// try to find a track in the db using the given artist, album and title
META_PUBLIC bool isNewTune(const QString &artist, const QString &album, const QString &title);
