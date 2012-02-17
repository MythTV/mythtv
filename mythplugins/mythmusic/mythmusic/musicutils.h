#include <QString>

// these are needed for QT4 compatibility
#undef QStringToTString
#define QStringToTString(s) TagLib::String(s.toUtf8().data(), TagLib::String::UTF8)
#undef TStringToQString
#define TStringToQString(s) QString::fromUtf8(s.toCString(true))

/// remove any bad filename characters
QString fixFilename(const QString &filename);

/// find an image for a artist or genre
QString findIcon(const QString &type, const QString &name);

/// calculate a tracks length by parsing the frames
uint calcTrackLength(const QString &musicFile);

/// create a filename using the template in the settings and a Metadata object
QString filenameFromMetadata(Metadata *track, bool createDir = true);

/// try to find a track in the db using the given artist, album and title
bool isNewTune(const QString &artist, const QString &album, const QString &title);
