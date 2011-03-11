//////////////////////////////////////////////////////////////////////////////
// Program Name: programs.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef PROGRAMS_H_
#define PROGRAMS_H_

#include <QDateTime>
#include <QString>
#include <QVariantList>

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "programAndChannel.h"

namespace DTC
{

class SERVICE_PUBLIC ProgramList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList Programs     READ Programs DESIGNABLE true )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , TotalAvailable  )    
    PROPERTYIMP       ( QDateTime   , AsOf            )
    PROPERTYIMP       ( QString     , Version         )
    PROPERTYIMP       ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, Programs      )

    public:

        ProgramList(QObject *parent = 0) 
            : QObject         ( parent ),
              m_StartIndex    ( 0      ),
              m_Count         ( 0      ),
              m_TotalAvailable( 0      )   
        {
        }
        
        ProgramList( const ProgramList &src ) 
        {
            Copy( src );
        }

        void Copy( const ProgramList &src )
        {
            m_StartIndex    = src.m_StartIndex     ;
            m_Count         = src.m_Count          ;
            m_TotalAvailable= src.m_TotalAvailable ;
            m_AsOf          = src.m_AsOf           ;
            m_Version       = src.m_Version        ;
            m_ProtoVer      = src.m_ProtoVer       ;
        
            CopyListContents< Program >( this, m_Programs, src.m_Programs );
        }

        Program *AddNewProgram()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            Program *pObject = new Program( this );
            m_Programs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ProgramList )

#endif
