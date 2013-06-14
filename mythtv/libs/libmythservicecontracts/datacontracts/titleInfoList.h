////////////////////////////////////////////////////////////////////////////
// Program Name: titleInfoList.h
// Created     : June 14, 2013
//
// Copyright (c) 2013 Chris Pinkham <cpinkham@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
////////////////////////////////////////////////////////////////////////////

#ifndef TITLEINFOLIST_H_
#define TITLEINFOLIST_H_

#include <QVariantList>

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "titleInfo.h"

namespace DTC
{

class SERVICE_PUBLIC TitleInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "TitleInfos", "type=DTC::TitleInfo");

    Q_PROPERTY( QVariantList TitleInfos READ TitleInfos DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, TitleInfos )

    public:

        static inline void InitializeCustomTypes();

        TitleInfoList(QObject *parent = 0) 
            : QObject( parent )               
        {
        }
        
        TitleInfoList( const TitleInfoList &src ) 
        {
            Copy( src );
        }

        void Copy( const TitleInfoList &src )
        {
            CopyListContents< TitleInfo >( this, m_TitleInfos, src.m_TitleInfos );
        }

        TitleInfo *AddNewTitleInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            TitleInfo *pObject = new TitleInfo( this );
            m_TitleInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::TitleInfoList  )
Q_DECLARE_METATYPE( DTC::TitleInfoList* )

namespace DTC
{
inline void TitleInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< TitleInfoList   >();
    qRegisterMetaType< TitleInfoList*  >();

    TitleInfo::InitializeCustomTypes();
}
}

#endif
