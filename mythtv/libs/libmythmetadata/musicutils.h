// qt
#include <QString>

// mythtv
#include "mythmetaexp.h"

class MusicMetadata;

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
