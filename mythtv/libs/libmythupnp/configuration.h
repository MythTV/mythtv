//////////////////////////////////////////////////////////////////////////////
// Program Name: configuration.h
// Created     : Feb. 12, 2007
//
// Purpose     : XML Configuration file Class
//                                                                            
// Copyright (c) 2007 David Blain <mythtv@theblains.net>
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

#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include <qdom.h>
#include <qstringlist.h>

class Configuration 
{
    public:

        virtual ~Configuration() {}

        virtual bool    Load    ( void ) = 0;
        virtual bool    Save    ( void ) = 0;

        virtual int     GetValue( const QString &sSetting, int     Default ) = 0; 
        virtual QString GetValue( const QString &sSetting, QString Default ) = 0; 

        virtual void    SetValue( const QString &sSetting, int     value   ) = 0; 
        virtual void    SetValue( const QString &sSetting, QString value   ) = 0; 

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

class XmlConfiguration : public Configuration
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

};

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class DBConfiguration : public Configuration
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

};

#endif

