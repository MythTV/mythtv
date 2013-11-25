////////////////////////////////////////////////////////////////////////////
// Program Name: titleInfo.h
// Created     : June 14, 2013
//
// Copyright (c) 2013 Chris Pinkham <cpinkham@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
////////////////////////////////////////////////////////////////////////////

#ifndef TITLEINFO_H_
#define TITLEINFO_H_

#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

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

    PROPERTYIMP    ( QString    , Title            )
    PROPERTYIMP    ( QString    , Inetref          )
    PROPERTYIMP    ( int        , Count            )

    public:

        static inline void InitializeCustomTypes();

        TitleInfo(QObject *parent = 0) 
            : QObject            ( parent )
        { 
        }
        
        TitleInfo( const TitleInfo &src )
        {
            Copy( src );
        }

        void Copy( const TitleInfo &src )
        {
            m_Title             = src.m_Title             ;
            m_Inetref           = src.m_Inetref           ;
            m_Count             = src.m_Count             ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::TitleInfo  )
Q_DECLARE_METATYPE( DTC::TitleInfo* )

namespace DTC
{
inline void TitleInfo::InitializeCustomTypes()
{
    qRegisterMetaType< TitleInfo   >();
    qRegisterMetaType< TitleInfo*  >();
}
}

#endif
