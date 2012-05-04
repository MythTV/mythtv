//////////////////////////////////////////////////////////////////////////////
// Program Name: configuration.h
// Created     : Feb. 12, 2007
//
// Purpose     : XML Configuration file Class
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include <QDomDocument>
#include <QStringList>

#include "upnpexp.h"

class UPNP_PUBLIC Configuration 
{
    public:

        virtual ~Configuration() {}

        virtual bool    Load    ( void ) = 0;
        virtual bool    Save    ( void ) = 0;

        virtual int     GetValue( const QString &sSetting, int     Default ) = 0; 
        virtual QString GetValue( const QString &sSetting, QString Default ) = 0; 

        virtual void    SetValue( const QString &sSetting, int     value   ) = 0; 
        virtual void    SetValue( const QString &sSetting, QString value   ) = 0; 
        virtual void    ClearValue( const QString &sSetting )                = 0;
};


//////////////////////////////////////////////////////////////////////////////
//
// **NOTE:  Performance Issue *** 
//
// This class loads an XML file into a QDomDocument.  All requests for 
// getting or setting values navigates the DOM each time.  All settings should 
// only be read/written once if possible.
//
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC XmlConfiguration : public Configuration
{
    protected:

        QString      m_sPath;
        QString      m_sFileName;

        QDomDocument m_config;
        QDomNode     m_rootNode;

        QDomNode FindNode( const QString &sName, bool bCreate = false );
        QDomNode FindNode( QStringList &sParts, QDomNode &curNode, bool bCreate = false );

    public:

        XmlConfiguration( const QString &sFileName );

        virtual ~XmlConfiguration() {}

        virtual bool    Load    ( void );
        virtual bool    Save    ( void );

        virtual int     GetValue( const QString &sSetting, int     Default ); 
        virtual QString GetValue( const QString &sSetting, QString Default ); 

        virtual void    SetValue( const QString &sSetting, int     value   ); 
        virtual void    SetValue( const QString &sSetting, QString value   ); 
        virtual void    ClearValue( const QString &sSetting );
};

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC DBConfiguration : public Configuration
{
    public:

        DBConfiguration();

        virtual ~DBConfiguration() {}

        virtual bool    Load    ( void );
        virtual bool    Save    ( void );

        virtual int     GetValue( const QString &sSetting, int     Default ); 
        virtual QString GetValue( const QString &sSetting, QString Default ); 

        virtual void    SetValue( const QString &sSetting, int     value   ); 
        virtual void    SetValue( const QString &sSetting, QString value   ); 
        virtual void    ClearValue( const QString &sSetting );
};

#endif

