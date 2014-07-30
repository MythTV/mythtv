//////////////////////////////////////////////////////////////////////////////
// Program Name: rtti.h
// Created     : July 25, 2014
//
// Copyright (c) 2014 David Blain <dblain@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef RTTI_H
#define RTTI_H

#include <QScriptEngine>
#include <QList>

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class RttiEnum : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_PROPERTY( QString                 Key          READ Key     WRITE setKey   )
    Q_PROPERTY( uint                    Value        READ Value   WRITE setValue )
    Q_PROPERTY( QString                 Desc         READ Desc    WRITE setDesc  )


    private:

        QString m_Key;
        int     m_Value;
        QString m_Desc;

    public:

        static inline void InitializeCustomTypes();

        QString    Key  () const              { return m_Key;   }
        int        Value() const              { return m_Value; }
        QString    Desc () const              { return m_Desc;  }


        void    setKey  ( const QString &val)  { m_Key   = val;  }
        void    setValue( const int      val)  { m_Value = val;  }
        void    setDesc ( const QString &val)  { m_Desc  = val;  }


    public:

        RttiEnum(QObject *parent = 0)
            : QObject           ( parent          ),
              m_Value           ( 0               )
        {
        }

        RttiEnum( const RttiEnum &src )
        {
            Copy( src );
        }

        void Copy( const RttiEnum &src )
        {
            m_Key    = src.m_Key  ;
            m_Value  = src.m_Value;
            m_Desc   = src.m_Desc ;
        }
};

//////////////////////////////////////////////////////////////////////////////

Q_DECLARE_METATYPE( RttiEnum  )
Q_DECLARE_METATYPE( RttiEnum* )

//////////////////////////////////////////////////////////////////////////////

inline void RttiEnum::InitializeCustomTypes()
{
    qRegisterMetaType< RttiEnum  >();
    qRegisterMetaType< RttiEnum* >();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class RttiEnumList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "RttiEnums", "type=RttiEnum");

    Q_PROPERTY( QVariantList RttiEnums READ RttiEnums DESIGNABLE true )

    private:

        QVariantList m_RttiEnums;

    public:

        QVariantList &RttiEnums()
        {
            return m_RttiEnums;
        }

        static inline void InitializeCustomTypes();

    public:

        RttiEnumList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        RttiEnumList( const RttiEnumList &src )
        {
            Copy( src );
        }

        void Copy( const RttiEnumList &src )
        {
            for( int nIdx = 0; nIdx < src.m_RttiEnums.size(); nIdx++ )
            {
                QVariant vValue = src.m_RttiEnums[ nIdx ];

                if ( vValue.canConvert< QObject* >())
                {
                    const QObject *pObject = vValue.value< QObject* >();

                    if (pObject != NULL)
                    {
                        QObject *pNew = new RttiEnum( this );

                        ((RttiEnum *)pNew)->Copy( (const RttiEnum &)(*pObject) );

                        m_RttiEnums.append( QVariant::fromValue<QObject *>( pNew ));
                    }
                }
            }
        }

        RttiEnum *AddNewRttiEnum()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            RttiEnum *pObject = new RttiEnum( this );
            m_RttiEnums.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};


Q_DECLARE_METATYPE( RttiEnumList  )
Q_DECLARE_METATYPE( RttiEnumList* )

inline void RttiEnumList::InitializeCustomTypes()
{
    qRegisterMetaType< RttiEnumList  >();
    qRegisterMetaType< RttiEnumList* >();

    RttiEnum::InitializeCustomTypes();
}

class Rtti
{
    public:
        Rtti();

        RttiEnumList* GetEnumDetails ( const QString &FQN );

};
#endif
