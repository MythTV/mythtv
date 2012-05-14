//////////////////////////////////////////////////////////////////////////////
// Program Name: configuration.cpp
// Created     : Feb. 12, 2007
//
// Purpose     : Configuration file Class
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include <unistd.h> // for fsync

#include <iostream>

#include <QDir>
#include <QFile>
#include <QTextStream>

#include "configuration.h"
#include "mythlogging.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "compat.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

XmlConfiguration::XmlConfiguration( const QString &sFileName )
{
    m_sPath     = GetConfDir();
    m_sFileName = sFileName;
    
    Load();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool XmlConfiguration::Load( void )
{
    QString sName = m_sPath + '/' + m_sFileName;

    QFile  file( sName );

    if (file.exists() && m_sFileName.length())  // Ignore empty filenames
    {

        if ( !file.open( QIODevice::ReadOnly ) )
            return false;

        QString sErrMsg;
        int     nErrLine = 0;
        int     nErrCol  = 0;
        bool    bSuccess = m_config.setContent(&file, false, &sErrMsg,
                                               &nErrLine, &nErrCol );

        file.close();

        if (!bSuccess)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error parsing: %1 at line: %2  column: %3")
                    .arg( sName ) .arg( nErrLine ) .arg( nErrCol  ));

            LOG(VB_GENERAL, LOG_ERR, QString("Error Msg: %1" ) .arg( sErrMsg ));
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
    if (m_sFileName.isEmpty())   // Special case. No file is created
        return true;

    QString config_temppath = m_sPath + '/' + m_sFileName + ".new";
    QString config_filepath = m_sPath + '/' + m_sFileName;
    QString config_origpath = m_sPath + '/' + m_sFileName + ".orig";

    QFile file(config_temppath);
    
    if (!file.exists())
    {
        QDir createDir(m_sPath);
        if (!createDir.exists())
        {
            if (!createDir.mkdir(m_sPath))
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Could not create %1").arg(m_sPath));
                return false;
            }
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Could not open settings file %1 for writing")
            .arg(config_temppath));

        return false;
    }

    {
        QTextStream ts(&file);
        m_config.save(ts, 2);
    }

    file.flush();

    fsync(file.handle());

    file.close();

    bool ok = true;
    if (QFile::exists(config_filepath))
        ok = QFile::rename(config_filepath, config_origpath);

    if (ok)
    {
        ok = file.rename(config_filepath);
        if (ok)
            QFile::remove(config_origpath);
        else if (QFile::exists(config_origpath))
            QFile::rename(config_origpath, config_filepath);
    }

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Could not save settings file %1").arg(config_filepath));
    }

    return ok;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QDomNode XmlConfiguration::FindNode( const QString &sName, bool bCreate )
{
    QStringList parts = sName.split('/', QString::SkipEmptyParts);

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
            if (!curNode.isNull())
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
    QDomNode node   = FindNode( sSetting, true );

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
    QDomNode node   = FindNode( sSetting, true );

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

void XmlConfiguration::ClearValue( const QString &sSetting )
{
    QDomNode node = FindNode(sSetting);
    if (!node.isNull())
    {
        QDomNode parent = node.parentNode();
        parent.removeChild(node);
        while (parent.childNodes().count() == 0)
        {
            QDomNode next_parent = parent.parentNode();
            next_parent.removeChild(parent);
            parent = next_parent;
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
    return GetMythDB()->GetNumSetting( sSetting, nDefault );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString DBConfiguration::GetValue( const QString &sSetting, QString sDefault ) 
{
    return GetMythDB()->GetSetting( sSetting, sDefault );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void DBConfiguration::SetValue( const QString &sSetting, int nValue ) 
{
    GetMythDB()->SaveSetting( sSetting, nValue );

}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void DBConfiguration::SetValue( const QString &sSetting, QString sValue ) 
{
    GetMythDB()->SaveSetting( sSetting, sValue );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void DBConfiguration::ClearValue( const QString &sSetting )
{
    GetMythDB()->ClearSetting( sSetting );
}
