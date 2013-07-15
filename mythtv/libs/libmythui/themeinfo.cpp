
// Own header
#include "themeinfo.h"

// QT headers
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QDomElement>

// MythTV headers
#include "mythlogging.h"
#include "mythdownloadmanager.h"

#define LOC      QString("ThemeInfo: ")

ThemeInfo::ThemeInfo(QString theme)
          :XMLParseBase()
{
    m_theme = QFileInfo(theme);
    m_type = THEME_UNKN;
    m_baseres = QSize(800, 600);
    m_majorver = m_minorver = 0;

    if (m_theme.exists())
        m_themeurl = m_theme.absoluteFilePath();
    else
        m_themeurl = theme;

    if (!parseThemeInfo())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("The theme (%1) is missing a themeinfo.xml file.")
                .arg(m_themeurl));
    }
}

ThemeInfo::~ThemeInfo()
{
}

bool ThemeInfo::parseThemeInfo()
{

    QDomDocument doc;

    if ((m_themeurl.startsWith("http://")) ||
        (m_themeurl.startsWith("https://")) ||
        (m_themeurl.startsWith("ftp://")) ||
        (m_themeurl.startsWith("myth://")))
    {
        QByteArray data;
        bool ok = GetMythDownloadManager()->download(m_themeurl +
                                                     "/themeinfo.xml", &data);
        if (!ok)
            return false;

        if (!doc.setContent(data))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Unable to parse themeinfo.xml for %1")
                        .arg(m_themeurl));
            return false;
        }
    }
    else
    {
        QFile f(m_themeurl + "/themeinfo.xml");

        if (!f.open(QIODevice::ReadOnly))
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Unable to open themeinfo.xml for %1")
                        .arg(f.fileName()));
            return false;
        }

        if (!doc.setContent(&f))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Unable to parse themeinfo.xml for %1")
                        .arg(f.fileName()));

            f.close();
            return false;
        }
        f.close();
    }

    QDomElement docElem = doc.documentElement();

    for (QDomNode n = docElem.firstChild(); !n.isNull();
            n = n.nextSibling())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "name")
            {
                m_name = getFirstText(e);
            }
            else if (e.tagName() == "aspect")
            {
                m_aspect = getFirstText(e);
            }
            else if (e.tagName() == "baseres")
            {
                    QString size = getFirstText(e);
                    m_baseres = QSize(size.section('x', 0, 0).toInt(),
                                            size.section('x', 1, 1).toInt());
            }
            else if (e.tagName() == "types")
            {
                for (QDomNode child = e.firstChild(); !child.isNull();
                        child = child.nextSibling())
                {
                    QDomElement ce = child.toElement();
                    if (!ce.isNull())
                    {
                        if (ce.tagName() == "type")
                        {
                            QString type = getFirstText(ce);

                            if (type == "UI")
                            {
                                m_type |= THEME_UI;
                            }
                            else if (type == "OSD")
                            {
                                m_type |= THEME_OSD;
                            }
                            else if (type == "Menu")
                            {
                                m_type |= THEME_MENU;
                            }
                            else
                            {
                                VERBOSE_XML(VB_GENERAL, LOG_ERR,
                                            m_theme.fileName(), ce, 
                                            "Invalid theme type");
                            }
                        }
                    }
                }
            }
            else if (e.tagName() == "version")
            {
                for (QDomNode child = e.firstChild(); !child.isNull();
                        child = child.nextSibling())
                {
                    QDomElement ce = child.toElement();
                    if (!ce.isNull())
                    {
                        if (ce.tagName() == "major")
                        {
                            m_majorver = getFirstText(ce).toInt();
                        }
                        else if (ce.tagName() == "minor")
                        {
                            m_minorver = getFirstText(ce).toInt();
                        }
                    }
                }
            }
            else if (e.tagName() == "author")
            {
                for (QDomNode child = e.firstChild(); !child.isNull();
                        child = child.nextSibling())
                {
                    QDomElement ce = child.toElement();
                    if (!ce.isNull())
                    {
                        if (ce.tagName() == "name")
                        {
                            m_authorName = getFirstText(ce);
                        }
                        else if (ce.tagName() == "email")
                        {
                            m_authorEmail = getFirstText(ce);
                        }
                    }
                }
            }
            else if (e.tagName() == "detail")
            {
                for (QDomNode child = e.firstChild(); !child.isNull();
                        child = child.nextSibling())
                {
                    QDomElement ce = child.toElement();
                    if (!ce.isNull())
                    {
                        if (ce.tagName() == "thumbnail")
                        {
                            if (ce.attribute("name") == "preview")
                            {
                                QString thumbnail = getFirstText(ce);
                                m_previewpath = m_themeurl + '/' + thumbnail;
                            }
                        }
                        else if (ce.tagName() == "description")
                        {
                            m_description = qApp->translate("ThemeUI",
                                                 parseText(ce).toUtf8(), NULL,
                                                 QCoreApplication::UnicodeUTF8);
                        }
                        else if (ce.tagName() == "errata")
                        {
                            m_errata = qApp->translate("ThemeUI",
                                                parseText(ce).toUtf8(), NULL,
                                                QCoreApplication::UnicodeUTF8);
                        }
                    }
                }
            }
            else if (e.tagName() == "downloadinfo")
            {
                for (QDomNode child = e.firstChild(); !child.isNull();
                        child = child.nextSibling())
                {
                    QDomElement ce = child.toElement();
                    if (!ce.isNull())
                    {
                        if (ce.tagName() == "url")
                        {
                            m_downloadurl = getFirstText(ce);
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool ThemeInfo::IsWide() const
{
    if (m_aspect == "16:9" || m_aspect == "16:10")
        return true;

    return false;
}

void ThemeInfo::ToMap(InfoMap &infoMap) const
{
    infoMap["description"] = m_description;
    infoMap["name"] = m_name;
    infoMap["aspect"] = m_aspect;
    infoMap["resolution"] = QString("%1x%2").arg(m_baseres.width())
                                            .arg(m_baseres.height());
    infoMap["errata"] = m_errata;
    infoMap["majorversion"] = m_majorver;
    infoMap["minorversion"] = m_minorver;
    infoMap["version"] = QString("%1.%2").arg(m_majorver).arg(m_minorver);

    infoMap["authorname"] = m_authorName;
    infoMap["authoremail"] = m_authorEmail;
}
