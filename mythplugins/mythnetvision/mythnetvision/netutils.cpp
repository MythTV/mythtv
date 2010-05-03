#include "netutils.h"

QString GetDisplaySeasonEpisode(int seasEp, int digits)
{
    QString seasEpNum = QString::number(seasEp);

    if (digits == 2 && seasEpNum.size() < 2)
        seasEpNum.prepend("0");

    return seasEpNum;
}

