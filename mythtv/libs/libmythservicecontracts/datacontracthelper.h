//////////////////////////////////////////////////////////////////////////////
// Program Name: datacontracthelper.h
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
//
//  * Copy Constructors (needed for Q_PROPERTY) don't do Deep Copies yet.
//  * DECLARE_METATYPE not working if this header is not included... need
//    to find solution since Serializer classes doesn't include this header.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef DATACONTRACTHELPER_H_
#define DATACONTRACTHELPER_H_

#include <QList>
#include <QStringList>
#include <QVariantMap>

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

#define PROPERTYIMP( type, name )       \
    private: type m_##name;             \
    public:                             \
    type name() const                   \
    {                                   \
        return m_##name;                \
    }                                   \
    void set##name(const type val)      \
    {                                   \
        m_##name = val;                 \
    }

//////////////////////////////////////////////////////////////////////////////

#define PROPERTYIMP_ENUM( type, name )  \
    private: type m_##name;             \
    public:                             \
    type name() const                   \
    {                                   \
        return m_##name;                \
    }                                   \
    void set##name(const type val)      \
    {                                   \
        m_##name = val;                 \
    }                                   \
    void set##name( int val)            \
    {                                   \
        m_##name = (type)val;           \
    }

//////////////////////////////////////////////////////////////////////////////

#define PROPERTYIMP_PTR( type, name )   \
    private: type* m_##name;            \
    public:                             \
    type* name()                        \
    {                                   \
        if (m_##name == NULL)           \
            m_##name = new type( this );\
        return m_##name;                \
    }                                   

//////////////////////////////////////////////////////////////////////////////

#define PROPERTYIMP_PTR_Old( type, name )   \
    private: type* m_##name;            \
    public:                             \
    type* name()                        \
    {                                   \
        return m_##name;                \
    }                                   \
    void set##name( QObject* val)       \
    {                                   \
        m_##name = qobject_cast< type* >( val ); \
    }                                   \
    void set##name( type* val)          \
    {                                   \
        m_##name = val;                 \
    }

//////////////////////////////////////////////////////////////////////////////

#define PROPERTYIMP_RO_REF( type, name ) \
    private: type m_##name;              \
    public:                              \
    type &name()                         \
    {                                    \
        return m_##name;                 \
    }                                    


namespace DTC
{

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

inline void DeleteListContents( QVariantList &list )
{
    while( !list.isEmpty() )
    {
        QVariant vValue = list.takeFirst();

        const QObject *pObject = vValue.value< QObject* >(); 

        if (pObject != NULL)
            delete pObject;
    }
}

/////////////////////////////////////////////////////////////////////////////

template< class T >
void CopyListContents( QObject *pParent, QVariantList &dst, const QVariantList &src )
{
    for( int nIdx = 0; nIdx < src.size(); nIdx++ )
    {
        QVariant vValue = src[ nIdx ];

        if ( vValue.canConvert< QObject* >()) 
    	{ 
		    const QObject *pObject = vValue.value< QObject* >(); 

            if (pObject != NULL)
            {
                QObject *pNew = new T( pParent );

                ((T *)pNew)->Copy( (const T &)(*pObject) );

                dst.append( QVariant::fromValue<QObject *>( pNew ));
            }
        }
    }
}

} // namespace DTC

#endif


