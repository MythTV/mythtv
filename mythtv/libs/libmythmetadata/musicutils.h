// qt
#include <QString>

// mythtv
#include "mythmetaexp.h"

class MusicMetadata;

// these are needed for QT4 compatibility
#undef QStringToTString
#define QStringToTString(s) TagLib::String(s.toUtf8().data(), TagLib::String::UTF8)
#undef TStringToQString
#define TStringToQString(s) QString::fromUtf8(s.toCString(true))

/// get music directory for this host
META_PUBLIC QString getMusicDirectory(void);

// get music directory for this host
META_PUBLIC void setMusicDirectory(const QString &musicDir);

/// remove any bad filename characters
META_PUBLIC QString fixFilename(const QString &filename);

/// find an image for a artist or genre
META_PUBLIC QString findIcon(const QString &type, const QString &name);

/// calculate a tracks length by parsing the frames
META_PUBLIC uint calcTrackLength(const QString &musicFile);

/// create a filename using the template in the settings and a MusicMetadata object
META_PUBLIC QString filenameFromMetadata(MusicMetadata *track, bool createDir = true);

/// try to find a track in the db using the given artist, album and title
META_PUBLIC bool isNewTune(const QString &artist, const QString &album, const QString &title);
