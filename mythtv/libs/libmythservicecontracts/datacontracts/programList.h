//////////////////////////////////////////////////////////////////////////////
// Program Name: programs.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef PROGRAMS_H_
#define PROGRAMS_H_

#include <QDateTime>
#include <QString>
#include <QVariantList>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

#include "programAndChannel.h"

namespace DTC
{

class SERVICE_PUBLIC ProgramList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Programs", "type=DTC::Program");
    Q_CLASSINFO( "AsOf"    , "transient=true"   );

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList Programs     READ Programs )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , TotalAvailable  )    
    PROPERTYIMP_REF   ( QDateTime   , AsOf            )
    PROPERTYIMP_REF   ( QString     , Version         )
    PROPERTYIMP_REF   ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, Programs      );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE explicit ProgramList(QObject *parent = nullptr)
            : QObject         ( parent ),
              m_StartIndex    ( 0      ),
              m_Count         ( 0      ),
              m_TotalAvailable( 0      )   
        {
        }

        void Copy( const ProgramList *src )
        {
            m_StartIndex    = src->m_StartIndex     ;
            m_Count         = src->m_Count          ;
            m_TotalAvailable= src->m_TotalAvailable ;
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;
        
            CopyListContents< Program >( this, m_Programs, src->m_Programs );
        }

        Program *AddNewProgram()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new Program( this );
            m_Programs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(ProgramList);
};

inline void ProgramList::InitializeCustomTypes()
{
    qRegisterMetaType< ProgramList* >();

    Program::InitializeCustomTypes();
}

} // namespace DTC

#endif
