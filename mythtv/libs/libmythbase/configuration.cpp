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


bool XmlConfiguration::FileExists()
{
    QString pathName = m_path + '/' + m_fileName;
    QFile file(pathName);
    return file.exists();
}

bool XmlConfiguration::Load()
{
    QString pathName = m_path + '/' + m_fileName;

    LOG(VB_GENERAL, LOG_DEBUG, QString("Loading %1").arg(pathName));

    QFile file(pathName);

    if (file.exists() && !m_fileName.isEmpty())  // Ignore empty filenames
    {

        if (!file.open(QIODevice::ReadOnly))
        {
            return false;
        }

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
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
#else
        auto parseresult = m_config.setContent(&file);

        file.close();

        if (!parseresult)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error parsing: %1 at line: %2  column: %3")
                .arg(pathName, QString::number(parseresult.errorLine),
                     QString::number(parseresult.errorColumn)));

            LOG(VB_GENERAL, LOG_ERR, QString("Error Msg: %1").arg(parseresult.errorMessage));
            return false;
        }
#endif

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
    QString backupName = pathName + ".old";

    LOG(VB_GENERAL, LOG_DEBUG, QString("Saving %1").arg(pathName));

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
        if (QFile::exists(backupName) && !QFile::remove(backupName)) // if true, rename will fail
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to remove '%1', cannot backup current settings").arg(backupName));
        }
        success = QFile::rename(pathName, backupName); // backup old settings in case it fails
    }

    if (success) // no settings to overwrite or settings backed up successfully
    {
        success = file.rename(pathName); // move new settings into target location
        if (success)
        {
            if (QFile::exists(backupName) && !QFile::remove(backupName))
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Failed to remove '%1'").arg(backupName));
            }
        }
        else if (QFile::exists(backupName) && !QFile::rename(backupName, pathName)) // !success &&
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to rename (restore) '%1").arg(backupName));
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
    QStringList path = setting.split('/', Qt::SkipEmptyParts);
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
    if (!node.isNull())
    {
        textNode = node.firstChild().toText();
        if (!textNode.isNull())
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Got \"%1\" for \"%2\"").arg(textNode.nodeValue(), setting));

            return textNode.nodeValue();
        }
    }

    LOG(VB_GENERAL, LOG_DEBUG, QString("Using default for \"%1\"").arg(setting));
    return {};
}

void XmlConfiguration::SetValue(const QString &setting, const QString& value)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Setting \"%1\" to \"%2\"").arg(setting, value));

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
    LOG(VB_GENERAL, LOG_DEBUG, QString("clearing %1").arg(setting));
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
