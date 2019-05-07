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

        void    BeginSerialize( QString &sName ) override; // XmlSerializer
        void    EndSerialize  () override; // XmlSerializer

        void    RenderValue     ( const QString &sName, const QVariant     &vValue , bool needKey = true);
        void    RenderStringList( const QString &sName, const QStringList  &list );
        void    RenderList      ( const QString &sName, const QVariantList &list );
        void    RenderMap       ( const QString &sName, const QVariantMap  &map  );

        void    BeginObject( const QString &sName, const QObject  *pObject ) override; // XmlSerializer
        void    EndObject  ( const QString &sName, const QObject  *pObject ) override; // XmlSerializer
        void    AddProperty( const QString       &sName,
                             const QVariant      &vValue,
                             const QMetaObject   *pMetaParent,
                             const QMetaProperty *pMetaProp ) override; // XmlSerializer

        void SerializePListObjectProperties( const QString &sName,
                                             const QObject *pObject,
                                                   bool    needKey );

    public:
        explicit XmlPListSerializer( QIODevice *pDevice )
            : XmlSerializer( pDevice, QString("") ) {}
        virtual ~XmlPListSerializer() = default;

        QString GetContentType() override; // XmlSerializer

};

#endif // XMLPLISTSERIALIZER_H
