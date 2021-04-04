//
//  mythtypes.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 16/07/13.
//

#include "mythtypes.h"

QString InfoMapToString(const InfoMap &infoMap, const QString &sep)
{
    QString str("");
    InfoMap::const_iterator it = infoMap.begin();
    for (; it != infoMap.end() ; ++it)
        str += QString("[%1]:%2%3").arg(it.key(), *it, sep);
    return str;
}
