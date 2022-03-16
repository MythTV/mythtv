////////////////////////////////////////////////////////////////////////////
// Program Name: titleInfo.h
// Created     : June 14, 2013
//
// Copyright (c) 2013 Chris Pinkham <cpinkham@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
////////////////////////////////////////////////////////////////////////////

#ifndef TITLEINFO_H_
#define TITLEINFO_H_

#include <QString>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC TitleInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.1" );

    Q_PROPERTY( QString         Title            READ Title             WRITE setTitle            )
    Q_PROPERTY( QString         Inetref          READ Inetref           WRITE setInetref          )
    Q_PROPERTY( int             Count            READ Count             WRITE setCount          )

    PROPERTYIMP_REF( QString    , Title            )
    PROPERTYIMP_REF( QString    , Inetref          )
    PROPERTYIMP    ( int        , Count            );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE TitleInfo(QObject *parent = nullptr)
            : QObject            ( parent ),
              m_Count(0)
        { 
        }

        void Copy( const TitleInfo *src )
        {
            m_Title             = src->m_Title             ;
            m_Inetref           = src->m_Inetref           ;
            m_Count             = src->m_Count             ;
        }

    private:
        Q_DISABLE_COPY(TitleInfo);
};

inline void TitleInfo::InitializeCustomTypes()
{
    qRegisterMetaType< TitleInfo*  >();
}

} // namespace DTC

#endif
