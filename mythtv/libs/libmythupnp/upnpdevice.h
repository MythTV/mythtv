//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpdevice.h
//                                                                            
// Purpose - UPnp Device Description parser/generator
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPDEVICE_H__
#define __UPNPDEVICE_H__

#include "upnpglobal.h"

#include <qdom.h>

class UPnpDevice;
class UPnpService;
class UPnpIcon;

/////////////////////////////////////////////////////////////////////////////
// Typedefs
/////////////////////////////////////////////////////////////////////////////

typedef QPtrList< UPnpDevice  >  UPnpDeviceList;
typedef QPtrList< UPnpService >  UPnpServiceList;
typedef QPtrList< UPnpIcon    >  UPnpIconList;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

class UPnpIcon
{
    public:

        QString     m_sMimeType;
        int         m_nWidth;
        int         m_nHeight;
        int         m_nDepth;
        QString     m_sURL;

        UPnpIcon() : m_nWidth ( 0 ), m_nHeight( 0 ), m_nDepth( 0 )  {}
};

/////////////////////////////////////////////////////////////////////////////

class UPnpService
{
    public:
        QString     m_sServiceType;
        QString     m_sServiceId;
        QString     m_sSCPDURL;
        QString     m_sControlURL;
        QString     m_sEventSubURL;        

        UPnpService() {}        
};

/////////////////////////////////////////////////////////////////////////////

class UPnpDevice
{
    public:

        QString         m_sDeviceType;
        QString         m_sFriendlyName;
        QString         m_sManufacturer;
        QString         m_sManufacturerURL;
        QString         m_sModelDescription;
        QString         m_sModelName;
        QString         m_sModelNumber;
        QString         m_sModelURL;
        QString         m_sSerialNumber;
        QString         m_sUPC;
        QString         m_sPresentationURL;

        UPnpIconList    m_listIcons;
        UPnpServiceList m_listServices;
        UPnpDeviceList  m_listDevices;

    public:

        UPnpDevice()
        {
            m_listIcons   .setAutoDelete( true );
            m_listServices.setAutoDelete( true );
            m_listDevices .setAutoDelete( true );
        }

        QString GetUDN()
        {
            return( "uuid:" + LookupUDN( m_sDeviceType ));
        }
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPnpDeviceDesc
{
    public:

        QString         m_sUPnpDescPath;
        UPnpDevice      m_rootDevice;

    protected: 

        void    _InternalLoad( QDomNode  oNode, UPnpDevice *pCurDevice );

        void     ProcessIconList   ( QDomNode oListNode, UPnpDevice *pDevice );
        void     ProcessServiceList( QDomNode oListNode, UPnpDevice *pDevice );

        void     OutputDevice( QTextStream &os, UPnpDevice *pDevice    );

        void     SetStrValue ( const QDomNode &n, QString &sValue );
        void     SetNumValue ( const QDomNode &n, int     &nValue );

        QString  FormatValue ( const QString &sName, const QString &sValue );
        QString  FormatValue ( const QString &sName, int nValue );

    public:

                 UPnpDeviceDesc();
        virtual ~UPnpDeviceDesc();

        bool     Load       ();

        void     GetValidXML( const QString &sBaseAddress, QTextStream &os  );
        QString  GetValidXML( const QString &sBaseAddress );

        QString  FindDeviceUDN( UPnpDevice *pDevice, QString sST );

};

#endif
