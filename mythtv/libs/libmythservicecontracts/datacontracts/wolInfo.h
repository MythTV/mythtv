//////////////////////////////////////////////////////////////////////////////
// Program Name: wolInfo.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef WOLINFO_H_
#define WOLINFO_H_

#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC WOLInfo : public QObject
{
    Q_OBJECT

    Q_CLASSINFO( "version"    , "1.0" );
    Q_CLASSINFO( "defaultProp", "Command" );

    Q_PROPERTY( bool     Enabled      READ Enabled   WRITE setEnabled   )
    Q_PROPERTY( int      Reconnect    READ Reconnect WRITE setReconnect )
    Q_PROPERTY( int      Retry        READ Retry     WRITE setRetry     )
    Q_PROPERTY( QString  Command      READ Command   WRITE setCommand   )

    PROPERTYIMP( bool   ,  Enabled   )
    PROPERTYIMP( int    ,  Reconnect )
    PROPERTYIMP( int    ,  Retry     )
    PROPERTYIMP( QString,  Command   )

    public:

        static inline void InitializeCustomTypes();

    public:

        WOLInfo(QObject *parent = 0) 
            : QObject    ( parent ),
              m_Enabled  ( false  ),
              m_Reconnect( 0      ),
              m_Retry    ( 0      )   
        {
        }
        
        WOLInfo( const WOLInfo &src ) 
        {
            Copy( src );
        }

        void Copy( const WOLInfo &src )
        {
            m_Enabled  = src.m_Enabled  ;
            m_Reconnect= src.m_Reconnect;
            m_Retry    = src.m_Retry    ;
            m_Command  = src.m_Command  ;       
        }
};

typedef WOLInfo* WOLInfoPtr;

} // namespace DTC

Q_DECLARE_METATYPE( DTC::WOLInfo  )
Q_DECLARE_METATYPE( DTC::WOLInfo* )

namespace DTC
{
inline void WOLInfo::InitializeCustomTypes()
{
    qRegisterMetaType< WOLInfo   >();
    qRegisterMetaType< WOLInfo*  >();
}
}

#endif
