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