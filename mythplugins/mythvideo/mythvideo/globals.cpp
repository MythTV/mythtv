#include <qobject.h>

#include "globals.h"

const QString VIDEO_CATEGORY_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_DIRECTOR_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_GENRE_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_COUNTRY_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_YEAR_UNKNOWN = QObject::tr("Unknown");
const QString VIDEO_RUNTIME_UNKNOWN = QObject::tr("Unknown");

const QString VIDEO_CATEGORY_DEFAULT = VIDEO_CATEGORY_UNKNOWN;
const QString VIDEO_DIRECTOR_DEFAULT = VIDEO_DIRECTOR_UNKNOWN;
const QString VIDEO_INETREF_DEFAULT = "00000000";
const QString VIDEO_COVERFILE_DEFAULT = QObject::tr("No Cover");
const QString VIDEO_RATING_DEFAULT = QObject::tr("NR");
const QString VIDEO_PLOT_DEFAULT = QObject::tr("None");

const QString JUMP_VIDEO_MANAGER = "Video Manager";
const QString JUMP_VIDEO_BROWSER = "Video Browser";
const QString JUMP_VIDEO_TREE = "Video Listings";
const QString JUMP_VIDEO_GALLERY = "Video Gallery";
const QString JUMP_VIDEO_DEFAULT = "MythVideo";

const int SCREEN_EXIT_VIA_JUMP = 5;

const QString DEFAULT_VIDEOSTARTUP_DIR = "/share/Movies/dvd";
