//////////////////////////////////////////////////////////////////////////////
// Program Name: configuration.cpp
// Created     : Feb. 12, 2007
//
// Purpose     : Configuration file Class
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#include "configuration.h"

#include <unistd.h> // for fsync

#include <iostream>

#include <QDir>
#include <QFile>
#include <QTextStream>

#include "mythlogging.h"
#include "mythdb.h"
#include "compat.h"

bool XmlConfiguration::Load()
{
    QString pathName = m_path + '/' + m_fileName;

    QFile file(pathName);

    if (file.exists() && !m_fileName.isEmpty())  // Ignore empty filenames
    {

        if (!file.open(QIODevice::ReadOnly))
        {
            return false;
        }

        QString error;
        int  line    = 0;
        int  column  = 0;
        bool success = m_config.setContent(&file, false, &error, &line, &column);

        file.close();

        if (!success)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error parsing: %1 at line: %2  column: %3")
                    .arg(pathName, QString::number(line), QString::number(column)));

            LOG(VB_GENERAL, LOG_ERR, QString("Error Msg: %1").arg(error));
            return false;
        }

        m_rootNode = m_config.namedItem("Configuration");
    }
    else
    {
        m_rootNode = m_config.createElement("Configuration");
        m_config.appendChild(m_rootNode);
    }

    return true;
}

bool XmlConfiguration::Save()
{
    if (m_fileName.isEmpty())   // Special case. No file is created
    {
        return true;
    }

    QString pathName = m_path + '/' + m_fileName;
    QString old      = pathName + ".old";

    QFile file(pathName + ".new");
    
    if (!file.exists())
    {
        QDir directory(m_path);
        if (!directory.exists() && !directory.mkdir(m_path))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Could not create %1").arg(m_path));
            return false;
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Could not open settings file %1 for writing").arg(file.fileName()));
        return false;
    }

    {
        QTextStream ts(&file);
        m_config.save(ts, 2);
    }

    file.flush();
    fsync(file.handle());
    file.close();

    bool success = true;
    if (QFile::exists(pathName))
    {
        if (QFile::exists(old) && !QFile::remove(old)) // if true, rename will fail
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to remove '%1', cannot backup current settings").arg(old));
        }
        success = QFile::rename(pathName, old); // backup old settings in case it fails
    }

    if (success) // no settings to overwrite or settings backed up successfully
    {
        success = file.rename(pathName); // move new settings into target location
        if (success)
        {
            if (QFile::exists(old) && !QFile::remove(old))
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Failed to remove '%1'").arg(old));
            }
        }
        else if (QFile::exists(old) && !QFile::rename(old, pathName)) // !success &&
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to rename (restore) '%1").arg(old));
        }
    }

    if (!success)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Could not save settings file %1").arg(pathName));
    }

    return success;
}

QDomNode XmlConfiguration::FindNode(const QString &setting, bool create)
{
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QStringList path = setting.split('/', QString::SkipEmptyParts);
#else
    QStringList path = setting.split('/', Qt::SkipEmptyParts);
#endif

    return FindNode(path, m_rootNode, create);

}

QDomNode XmlConfiguration::FindNode(QStringList &path, QDomNode &current, bool create)
{
    if (path.empty())
    {
        return current;
    }

    QString name = path.front();
    path.pop_front();

    QDomNode child = current.namedItem(name);

    if (child.isNull())
    {
        if (!create)
        {
            path.clear();
        }
        else if (!current.isNull()) // && create
        {
            child = current.appendChild(m_config.createElement(name));
        }
    }

    return FindNode(path, child, create);
}

QString XmlConfiguration::GetValue(const QString &setting)
{
    QDomNode node = FindNode(setting, false);
    QDomText textNode;
    // -=>TODO: This Always assumes firstChild is a Text Node... should change
    if (!node.isNull() && !(textNode = node.firstChild().toText()).isNull())
    {
        return textNode.nodeValue();
    }
    return QString();
}

void XmlConfiguration::SetValue(const QString &setting, const QString& value)
{
    QDomNode node = FindNode(setting, true);

    if (!node.isNull())
    {
        QDomText textNode;

        if (node.hasChildNodes())
        {
            // -=>TODO: This Always assumes only child is a Text Node... should change
            textNode = node.firstChild().toText();
            textNode.setNodeValue(value);
        }
        else
        {
            textNode = m_config.createTextNode(value);
            node.appendChild(textNode);
        }
    }
}

void XmlConfiguration::ClearValue(const QString &setting)
{
    QDomNode node = FindNode(setting, false);
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
