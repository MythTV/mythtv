#include "backendcontext.h"

QMap<int, EncoderLink *> tvList;
AutoExpire  *expirer      = NULL;
JobQueue    *jobqueue     = NULL;
HouseKeeper *housekeeping = NULL;
MediaServer *g_pUPnp      = NULL;
QString      pidfile;
QString      logfile;
