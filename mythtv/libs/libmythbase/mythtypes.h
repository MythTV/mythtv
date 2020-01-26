//
//  mythtypes.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 16/07/13.
//

#ifndef MYTHTV_MYTHTYPES_H
#define MYTHTV_MYTHTYPES_H

#include <QObject>
#include <QHash>
#include "mythbaseexp.h"

using InfoMap = QHash<QString,QString>;

QString InfoMapToString(const InfoMap &infoMap, const QString &sep="\n");

#endif /* defined(MYTHTV_MYTHTYPES_H) */
