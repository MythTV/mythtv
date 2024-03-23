//////////////////////////////////////////////////////////////////////////////
// Program Name: datacontracthelper.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
//
//  * Copy Constructors (needed for Q_PROPERTY) don't do Deep Copies yet.
//
//  * DECLARE_METATYPE not working if this header is not included... need
//    to find solution since Serializer classes doesn't include this header.
//
//  * Q_CLASSINFO is used to add metadata options to individual properties
//    the the Qt Metadata system doesn't account for.  
//    It can be used in each data contract class.  The format is as follows:
//
//        Q_CLASSINFO( "<PropName>", "<option>=<value>;<option>=<value" );
//
//    Valid options/values are:
//
//       type=<containedType> - used by collections to know what type of
//                              object is stored. (QVariantMap, QVariantList)
//       name=<name>          - used for QVariantMap & QVariantList to hint
//                              to the serializer what name to use for each
//                              object in collection (child element in XML).
//       transient=true       - If present, this property will not be used
//                              when calculating the SHA1 hash used for ETag
//                              http header.
//
//
//  * DESIGNABLE in Q_PROPERTY is used to indicate if it should be Serialized 
//    (can specify a propery to support runtime logic)
//
//    N.B., DESIGNABLE was removed in v32-Pre to prepare for Qt 6.
//
//
//  * Q_CLASSINFO( "defaultProp", "<propname>" ) is used to indicate the 
//    default property (used for node text in XML)
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

#define PROPERTYIMP_REF( type, name )   \
    private: type m_##name;             \
    public:                             \
    type name() const                   \
    {                                   \
        return m_##name;                \
    }                                   \
    void set##name(const type& val)     \
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
    private: type* m_##name;              /* NOLINT(bugprone-macro-parentheses) */ \
    public:                             \
    type* name()                          /* NOLINT(bugprone-macro-parentheses) */ \
    {                                   \
        if (m_##name == nullptr)        \
            m_##name = new type( this );\
        return m_##name;                \
    }

//////////////////////////////////////////////////////////////////////////////

#define PROPERTYIMP_RO_REF( type, name ) \
    private: type m_##name;              \
    public:                              \
    type &name()                           /* NOLINT(bugprone-macro-parentheses) */ \
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

        delete pObject;
    }
}

/////////////////////////////////////////////////////////////////////////////

template< class T >
void CopyListContents( QObject *pParent, QVariantList &dst, const QVariantList &src )
{
    for (const auto& vValue : std::as_const(src))
    {
        if ( vValue.canConvert< QObject* >())
        {
            const QObject *pObject = vValue.value< QObject* >();

            if (pObject != nullptr)
            {
                QObject *pNew = new T( pParent );

                ((T *)pNew)->Copy( (const T *)pObject );

                dst.append( QVariant::fromValue<QObject *>( pNew ));
            }
        }
    }
}

} // namespace DTC

#endif


