////////////////////////////////////////////////////////////////////////////
// Program Name: titleInfoList.h
// Created     : June 14, 2013
//
// Copyright (c) 2013 Chris Pinkham <cpinkham@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
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

    Q_PROPERTY( QVariantList TitleInfos READ TitleInfos )

    PROPERTYIMP_RO_REF( QVariantList, TitleInfos );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE TitleInfoList(QObject *parent = nullptr)
            : QObject( parent )               
        {
        }

        void Copy( const TitleInfoList *src )
        {
            CopyListContents< TitleInfo >( this, m_TitleInfos, src->m_TitleInfos );
        }

        TitleInfo *AddNewTitleInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new TitleInfo( this );
            m_TitleInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(TitleInfoList);
};

inline void TitleInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< TitleInfoList*  >();

    TitleInfo::InitializeCustomTypes();
}

} // namespace DTC

#endif
