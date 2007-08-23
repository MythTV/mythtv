//////////////////////////////////////////////////////////////////////////////
// Program Name: configuration.h
//                                                                            
// Purpose - XML Configuration file Class
//                                                                            
// Created By  : David Blain                    Created On : Feb. 12, 2007
// Modified By :                                Modified On:                  
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

        QDomNode FindNode( const QString &sName, bool bCreate = FALSE );
        QDomNode FindNode( QStringList &sParts, QDomNode &curNode, bool bCreate = FALSE );

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

