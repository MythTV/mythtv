#include <QObject>
#include <QDir>

#include "globals.h"

const QString VIDEO_CATEGORY_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_DIRECTOR_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_GENRE_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_COUNTRY_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_YEAR_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_RUNTIME_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_CAST_UNKNOWN = QObject::tr("Unknown");

const QString VIDEO_CATEGORY_DEFAULT = VIDEO_CATEGORY_UNKNOWN;
const QString VIDEO_DIRECTOR_DEFAULT = VIDEO_DIRECTOR_UNKNOWN;
const QString VIDEO_INETREF_DEFAULT = "00000000";
const QString VIDEO_COVERFILE_DEFAULT;
const QString VIDEO_TRAILER_DEFAULT;
const QString VIDEO_SCREENSHOT_DEFAULT;
const QString VIDEO_BANNER_DEFAULT;
const QString VIDEO_FANART_DEFAULT;
const QString VIDEO_RATING_DEFAULT = QObject::tr("NR");
const QString VIDEO_PLOT_DEFAULT = QObject::tr("None");
const QString VIDEO_CAST_DEFAULT = VIDEO_CAST_UNKNOWN;

const QString JUMP_VIDEO_MANAGER = "Video Manager";
const QString JUMP_VIDEO_BROWSER = "Video Browser";
const QString JUMP_VIDEO_TREE = "Video Listings";
const QString JUMP_VIDEO_GALLERY = "Video Gallery";
const QString JUMP_VIDEO_DEFAULT = "Video Default";

#ifdef Q_OS_MAC
const QString DEFAULT_VIDEOSTARTUP_DIR = QDir::homePath() + "/Movies";
#else
const QString DEFAULT_VIDEOSTARTUP_DIR = "/share/Movies/dvd";
#endif
