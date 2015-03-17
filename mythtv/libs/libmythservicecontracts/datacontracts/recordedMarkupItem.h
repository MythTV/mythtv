//////////////////////////////////////////////////////////////////////////////
// Program Name: recordedMarkupItem.h
// Created     : Apr. 17, 2013
//
// Copyright (c) 2013 Tikinou LLC <dev-team@tikinou.com>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef RECORDEDMARKUPITEM_H_
#define RECORDEDMARKUPITEM_H_

#include "serviceexp.h" 
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC RecordedMarkupItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString         Type            READ Type             WRITE setType           )
    Q_PROPERTY( int             Start           READ Start            WRITE setStart          )
    Q_PROPERTY( int             End             READ End              WRITE setEnd            )

    PROPERTYIMP         ( int        , Start           )
    PROPERTYIMP         ( int        , End             )
    PROPERTYIMP         ( QString    , Type            )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< RecordedMarkupItem  >();
            qRegisterMetaType< RecordedMarkupItem* >();
        }

    public:

        RecordedMarkupItem(QObject *parent = 0)
            : QObject             ( parent ),
              m_Start             ( 0             ),
              m_End               ( 0             ),
              m_Type              ( QString("")   )
        { 
        }
        
        RecordedMarkupItem( const RecordedMarkupItem &src )
        {
            Copy( src );
        }

        void Copy( const RecordedMarkupItem &src )
        {
        	m_Start               = src.m_Start               ;
        	m_End                 = src.m_End                 ;
        	m_Type                = src.m_Type                ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::RecordedMarkupItem  )
Q_DECLARE_METATYPE( DTC::RecordedMarkupItem* )

#endif
