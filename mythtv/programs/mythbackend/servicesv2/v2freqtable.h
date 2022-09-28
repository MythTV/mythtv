//////////////////////////////////////////////////////////////////////////////
// Program Name: v2freqtable.h
// Created     : Sep 22, 2022
//
// Copyright (c) 2022 Peter Bennett <pbennett@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2FREQTABLE_H_
#define V2FREQTABLE_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////


class V2FreqTableList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    SERVICE_PROPERTY2( QStringList,  FreqTables )

    public:

        Q_INVOKABLE V2FreqTableList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        // Copy not needed


    private:
        Q_DISABLE_COPY(V2FreqTableList);
};

Q_DECLARE_METATYPE(V2FreqTableList*)

#endif
