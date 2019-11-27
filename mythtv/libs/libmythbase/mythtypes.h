//
//  mythtypes.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 16/07/13.
//

#ifndef __MythTV__mythtypes__
#define __MythTV__mythtypes__

#include <QObject>
#include <QHash>
#include "mythbaseexp.h"

using InfoMap = QHash<QString,QString>;

QString InfoMapToString(const InfoMap &infoMap, const QString &sep="\n");

#endif /* defined(__MythTV__mythtypes__) */
