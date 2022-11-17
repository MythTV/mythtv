//////////////////////////////////////////////////////////////////////////////
// Program Name: captureCardList.h
// Created     : Sep. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2CAPTURECARDLIST_H_
#define V2CAPTURECARDLIST_H_

#include <QString>
#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2captureCard.h"

class V2CaptureCardList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "CaptureCards", "type=V2CaptureCard");

    SERVICE_PROPERTY2( QVariantList, CaptureCards );

    public:

        Q_INVOKABLE V2CaptureCardList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2CaptureCardList *src )
        {
            CopyListContents< V2CaptureCard >( this, m_CaptureCards, src->m_CaptureCards );
        }

        V2CaptureCard *AddNewCaptureCard()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2CaptureCard( this );
            m_CaptureCards.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2CaptureCardList);
};

Q_DECLARE_METATYPE(V2CaptureCardList*)

class V2CardTypeList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "CardTypes", "type=V2CardType");

    SERVICE_PROPERTY2( QVariantList, CardTypes );

    public:

        Q_INVOKABLE V2CardTypeList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2CardTypeList *src )
        {
            CopyListContents< V2CardTypeList >( this, m_CardTypes, src->m_CardTypes );
        }

        V2CardType *AddCardType(const QString &description, const QString &cardType)
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2CardType( this );
            pObject->setCardType(cardType);
            pObject->setDescription(description);
            m_CardTypes.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2CardTypeList);
};

Q_DECLARE_METATYPE(V2CardTypeList*)

class V2CaptureDeviceList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "CaptureDevices", "type=V2CaptureDevice");

    SERVICE_PROPERTY2( QVariantList, CaptureDevices );

    public:

        Q_INVOKABLE V2CaptureDeviceList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2CaptureDeviceList *src )
        {
            CopyListContents< V2CaptureDeviceList >( this, m_CaptureDevices, src->m_CaptureDevices );
        }

        V2CaptureDevice *AddCaptureDevice()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2CaptureDevice( this );
            m_CaptureDevices.append( QVariant::fromValue<QObject *>( pObject ));
            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2CaptureDeviceList);
};

Q_DECLARE_METATYPE(V2CaptureDeviceList*)


class V2InputGroupList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "InputGroups", "type=V2InputGroup");

    SERVICE_PROPERTY2( QVariantList, InputGroups );

    public:

        Q_INVOKABLE V2InputGroupList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2InputGroupList *src )
        {
            CopyListContents< V2InputGroupList >( this, m_InputGroups,
                                                  src->m_InputGroups );
        }

        V2InputGroup *AddInputGroup(uint cardInputId, uint inputGroupId,
                                    const QString &inputGroupName)
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2InputGroup( this );
            pObject->setCardInputId(cardInputId);
            pObject->setInputGroupId(inputGroupId);
            pObject->setInputGroupName(inputGroupName);
            m_InputGroups.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2InputGroupList);
};

Q_DECLARE_METATYPE(V2InputGroupList*)


class V2DiseqcTreeList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "DiseqcTrees", "type=V2DiseqcTree");

    SERVICE_PROPERTY2( QVariantList, DiseqcTrees );

    public:

        Q_INVOKABLE V2DiseqcTreeList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2DiseqcTreeList *src )
        {
            CopyListContents< V2DiseqcTreeList >( this, m_DiseqcTrees, src->m_DiseqcTrees );
        }

        V2DiseqcTree *AddDiseqcTree()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2DiseqcTree( this );
            m_DiseqcTrees.append( QVariant::fromValue<QObject *>( pObject ));
            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2DiseqcTreeList);
};

Q_DECLARE_METATYPE(V2DiseqcTreeList*)

class V2DiseqcConfigList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "DiseqcConfigs", "type=V2DiseqcConfig");

    SERVICE_PROPERTY2( QVariantList, DiseqcConfigs );

    public:

        Q_INVOKABLE V2DiseqcConfigList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2DiseqcConfigList *src )
        {
            CopyListContents< V2DiseqcConfigList >( this, m_DiseqcConfigs, src->m_DiseqcConfigs );
        }

        V2DiseqcConfig *AddDiseqcConfig()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2DiseqcConfig( this );
            m_DiseqcConfigs.append( QVariant::fromValue<QObject *>( pObject ));
            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2DiseqcConfigList);
};

Q_DECLARE_METATYPE(V2DiseqcConfigList*)



#endif
