//////////////////////////////////////////////////////////////////////////////
// Program Name: configuration.cpp
//                                                                            
// Purpose - Configuration file Class
//                                                                            
// Created By  : David Blain                    Created On : Feb. 12, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "configuration.h"
#include "mythcontext.h"

#include <qfile.h>
#include <qdir.h>

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

XmlConfiguration::XmlConfiguration( const QString &sFileName )
{
    m_sPath     = MythContext::GetConfDir();
    m_sFileName = sFileName;
    
    Load();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool XmlConfiguration::Load( void )
{
    QString sName = m_sPath + "/" + m_sFileName;

    QFile  file( sName );

    if (file.exists())
    {

        if ( !file.open( IO_ReadOnly ) )
            return false;

        QString sErrMsg;
        int     nErrLine = 0;
        int     nErrCol  = 0;
        bool    bSuccess = m_config.setContent( &file, false, &sErrMsg, &nErrLine, &nErrCol );

        file.close();

        if (!bSuccess)
        {
            VERBOSE(VB_IMPORTANT, QString("Configuration::Load - "
                                          "Error parsing: %1 "
                                          "at line: %2  column: %3")
                                     .arg( sName )
                                     .arg( nErrLine )
                                     .arg( nErrCol  ));

            VERBOSE(VB_IMPORTANT, QString("Configuration::Load - Error Msg: %1" )
                                     .arg( sErrMsg ));
            return false;
        }

        m_rootNode = m_config.namedItem( "Configuration" );
    }
    else
    {
        m_rootNode = m_config.createElement( "Configuration" );
        m_config.appendChild( m_rootNode );
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool XmlConfiguration::Save( void )
{

    QString sName = m_sPath + "/" + m_sFileName;

    QFile file( sName );
    
    if (!file.exists())
    {
        QDir createDir( m_sPath );

        if (!createDir.exists())
        {
            if (!createDir.mkdir( m_sPath, true ))
            {
                VERBOSE(VB_IMPORTANT, QString("Could not create %1").arg( m_sPath ));
                return false;
            }
        }
    }

    if (!file.open( IO_WriteOnly | IO_Truncate ))
    {
        VERBOSE(VB_IMPORTANT, QString("Could not open settings file %1 "
                                      "for writing").arg( sName ));

        return false;
    }

    QTextStream ts( &file );

    m_config.save( ts, 2 );

    file.close();

    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QDomNode XmlConfiguration::FindNode( const QString &sName, bool bCreate )
{
    QStringList parts = QStringList::split( "/", sName );

    return FindNode( parts, m_rootNode, bCreate );

}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QDomNode XmlConfiguration::FindNode( QStringList &sParts, QDomNode &curNode, bool bCreate )
{
    if (sParts.empty())
        return curNode;

    QString sName = sParts.front();
    sParts.pop_front();

    QDomNode child = curNode.namedItem( sName );

    if (child.isNull() )
    {
        if (bCreate)
        {
            QDomNode newNode = m_config.createElement( sName );

            child = curNode.appendChild( newNode );
        }
        else
            sParts.clear();
    }

    return FindNode( sParts, child, bCreate );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

int XmlConfiguration::GetValue( const QString &sSetting, int nDefault )
{
    QDomNode node = FindNode( sSetting );

    if (!node.isNull())
    {
        // -=>TODO: This Always assumes firstChild is a Text Node... should change
        QDomText  oText = node.firstChild().toText();

        if (!oText.isNull())
            return oText.nodeValue().toInt();
    }

    return nDefault;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString XmlConfiguration::GetValue( const QString &sSetting, QString sDefault ) 
{
    QDomNode node = FindNode( sSetting );

    if (!node.isNull())
    {
        // -=>TODO: This Always assumes firstChild is a Text Node... should change
        QDomText  oText = node.firstChild().toText();

        if (!oText.isNull())
            return oText.nodeValue();
    }

    return sDefault;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlConfiguration::SetValue( const QString &sSetting, int nValue ) 
{
    QString  sValue = QString::number( nValue );
    QDomNode node   = FindNode( sSetting, TRUE );

    if (!node.isNull())
    {
        QDomText textNode;

        if (node.hasChildNodes())
        {
            // -=>TODO: This Always assumes only child is a Text Node... should change
            textNode = node.firstChild().toText();
            textNode.setNodeValue( sValue );
        }
        else
        {
            textNode = m_config.createTextNode( sValue );
            node.appendChild( textNode );
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void XmlConfiguration::SetValue( const QString &sSetting, QString sValue ) 
{
    QDomNode node   = FindNode( sSetting, TRUE );

    if (!node.isNull())
    {
        QDomText textNode;

        if (node.hasChildNodes())
        {
            // -=>TODO: This Always assumes only child is a Text Node... should change
            textNode = node.firstChild().toText();
            textNode.setNodeValue( sValue );
        }
        else
        {
            textNode = m_config.createTextNode( sValue );
            node.appendChild( textNode );
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// Uses MythContext to access settings in Database
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

DBConfiguration::DBConfiguration( ) 
{
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool DBConfiguration::Load( void )
{
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool DBConfiguration::Save( void )
{
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

int DBConfiguration::GetValue( const QString &sSetting, int nDefault )
{
    return gContext->GetNumSetting( sSetting, nDefault );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString DBConfiguration::GetValue( const QString &sSetting, QString sDefault ) 
{
    return gContext->GetSetting( sSetting, sDefault );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void DBConfiguration::SetValue( const QString &sSetting, int nValue ) 
{
    gContext->SaveSetting( sSetting, nValue );

}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void DBConfiguration::SetValue( const QString &sSetting, QString sValue ) 
{
    gContext->SaveSetting( sSetting, sValue );
}
