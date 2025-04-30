//////////////////////////////////////////////////////////////////////////////
// Program Name: upnp.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Main Class
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPNP_H
#define UPNP_H

// Qt
#include <QObject>

// MythTV
#include "libmythbase/mythpower.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

#include "upnpdevice.h"
#include "httpserver.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

enum UPnPResultCode : std::uint16_t
{
    UPnPResult_Success                       =   0,

    UPnPResult_InvalidAction                 = 401,
    UPnPResult_InvalidArgs                   = 402,
    UPnPResult_ActionFailed                  = 501,
    UPnPResult_ArgumentValueInvalid          = 600,
    UPnPResult_ArgumentValueOutOfRange       = 601,
    UPnPResult_OptionalActionNotImplemented  = 602,
    UPnPResult_OutOfMemory                   = 603,
    UPnPResult_HumanInterventionRequired     = 604,
    UPnPResult_StringArgumentTooLong         = 605,
    UPnPResult_ActionNotAuthorized           = 606,
    UPnPResult_SignatureFailure              = 607,
    UPnPResult_SignatureMissing              = 608,
    UPnPResult_NotEncrypted                  = 609,
    UPnPResult_InvalidSequence               = 610,
    UPnPResult_InvalidControlURL             = 611,
    UPnPResult_NoSuchSession                 = 612,

    UPnPResult_CDS_NoSuchObject              = 701,
    UPnPResult_CDS_InvalidCurrentTagValue    = 702,
    UPnPResult_CDS_InvalidNewTagValue        = 703,
    UPnPResult_CDS_RequiredTag               = 704,
    UPnPResult_CDS_ReadOnlyTag               = 705,
    UPnPResult_CDS_ParameterMismatch         = 706,
    UPnPResult_CDS_InvalidSearchCriteria     = 708,
    UPnPResult_CDS_InvalidSortCriteria       = 709,
    UPnPResult_CDS_NoSuchContainer           = 710,
    UPnPResult_CDS_RestrictedObject          = 711,
    UPnPResult_CDS_BadMetadata               = 712,
    UPnPResult_CDS_ResrtictedParentObject    = 713,
    UPnPResult_CDS_NoSuchSourceResource      = 714,
    UPnPResult_CDS_ResourceAccessDenied      = 715,
    UPnPResult_CDS_TransferBusy              = 716,
    UPnPResult_CDS_NoSuchFileTransfer        = 717,
    UPnPResult_CDS_NoSuchDestRes             = 718,
    UPnPResult_CDS_DestResAccessDenied       = 719,
    UPnPResult_CDS_CannotProcessRequest      = 720,

    UPnPResult_CMGR_IncompatibleProtocol     = 701,
    UPnPResult_CMGR_IncompatibleDirections   = 702,
    UPnPResult_CMGR_InsufficientNetResources = 703,
    UPnPResult_CMGR_LocalRestrictions        = 704,
    UPnPResult_CMGR_AccessDenied             = 705,
    UPnPResult_CMGR_InvalidConnectionRef     = 706,
    UPnPResult_CMGR_NotInNetwork             = 707,

    UPnPResult_MS_AccessDenied               = 801,

    UPnPResult_MythTV_NoNamespaceGiven       = 32001,
    UPnPResult_MythTV_XmlParseError          = 32002,
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnp : public QObject
{
    Q_OBJECT

    protected:

        HttpServer             *m_pHttpServer  {nullptr};
        int                     m_nServicePort {0};

    public:

        static UPnpDeviceDesc       g_UPnpDeviceDesc;
        static QList<QHostAddress>  g_IPAddrList;

    public:
        UPnp();
       ~UPnp() override;

        bool Initialize( int nServicePort, HttpServer *pHttpServer );
        bool Initialize( QList<QHostAddress> &sIPAddrList, int nServicePort,
                         HttpServer *pHttpServer );

        bool isInitialized() { return (m_pHttpServer != nullptr); }

        virtual void Start();

        static void CleanUp      ();

        static UPnpDevice *RootDevice() { return &(g_UPnpDeviceDesc.m_rootDevice); }

        HttpServer *GetHttpServer() { return m_pHttpServer; }

        static UPnpDeviceDesc *GetDeviceDesc( QString &sURL );

        static QString         GetResultDesc( UPnPResultCode eCode );
        static void            FormatErrorResponse( HTTPRequest   *pRequest, 
                                                    UPnPResultCode  eCode, 
                                                    const QString &sMsg = "" );

        static void            FormatRedirectResponse( HTTPRequest   *pRequest,
                                                       const QString &hostName );

    public slots:
        static void DisableNotifications(std::chrono::milliseconds /*unused*/);
        void EnableNotificatins(std::chrono::milliseconds /*unused*/) const;

    private:
        MythPower* m_power;
};

#endif // UPNP_H
