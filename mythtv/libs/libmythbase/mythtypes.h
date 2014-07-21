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

typedef QHash<QString,QString> InfoMap;

typedef struct TextProperties {
    QString text;
    QString state;
} TextProperties;

#endif /* defined(__MythTV__mythtypes__) */
