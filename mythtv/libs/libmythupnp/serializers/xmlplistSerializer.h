#ifndef XMLPLISTSERIALIZER_H
#define XMLPLISTSERIALIZER_H

#include <QXmlStreamWriter>
#include <QVariant>
#include <QIODevice>
#include <QStringList>

#include "upnpexp.h"
#include "xmlSerializer.h"

class UPNP_PUBLIC XmlPListSerializer : public XmlSerializer
{

    protected:

        virtual void BeginSerialize( QString &sName );
        virtual void EndSerialize  ();

        void    RenderValue     ( const QString &sName, const QVariant     &vValue , bool needKey = true);
        void    RenderStringList( const QString &sName, const QStringList  &list );
        void    RenderList      ( const QString &sName, const QVariantList &list );
        void    RenderMap       ( const QString &sName, const QVariantMap  &map  );

        virtual void BeginObject( const QString &sName, const QObject  *pObject );
        virtual void EndObject  ( const QString &sName, const QObject  *pObject );
        virtual void AddProperty( const QString       &sName,
                                  const QVariant      &vValue,
                                  const QMetaObject   *pMetaParent,
                                  const QMetaProperty *pMetaProp );

        void SerializePListObjectProperties( const QString &sName,
                                             const QObject *pObject,
                                                   bool    needKey );

    public:
                 XmlPListSerializer( QIODevice *pDevice );
        virtual ~XmlPListSerializer();

        virtual QString GetContentType();

};

#endif // XMLPLISTSERIALIZER_H
