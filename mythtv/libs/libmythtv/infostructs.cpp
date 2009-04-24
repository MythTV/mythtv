#include <qstring.h>
#include <qapplication.h>

#include "infostructs.h"
#include "mythcontext.h"

QString ChannelInfo::Text(QString format)
{
    format.replace("<num>", chanstr);
    format.replace("<sign>", callsign);
    format.replace("<name>", channame);

    return format;
}

