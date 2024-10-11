
// Own header
#include "themeinfo.h"

// QT headers
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QDomElement>

// MythTV headers
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"

#define LOC      QString("ThemeInfo: ")

ThemeInfo::ThemeInfo(const QString& theme)
          :XMLParseBase()
{
    QString themeNoTrailingSlash = theme;
    if (themeNoTrailingSlash.endsWith('/'))
    {
        themeNoTrailingSlash.chop(1);
    }
    m_theme = QFileInfo(themeNoTrailingSlash);

    if (m_theme.exists())
    {
        // since all the usages have a / inserted, remove the one in the url
        m_themeurl = m_theme.absoluteFilePath();
    }
    else
    {
        m_themeurl = theme;
    }

    // since all the usages have a / insterted, remove the one in the url
    if (m_themeurl.endsWith('/'))
    {
        m_themeurl.chop(1);
    }

    if (!parseThemeInfo())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("The theme (%1) is missing a themeinfo.xml file.")
                .arg(m_themeurl));
    }
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
            else if (e.tagName() == "basetheme")
            {
                m_baseTheme = getFirstText(e);
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
                            m_description = QCoreApplication::translate("ThemeUI",
                                                 parseText(ce).toUtf8());
                        }
                        else if (ce.tagName() == "errata")
                        {
                            m_errata = QCoreApplication::translate("ThemeUI",
                                                parseText(ce).toUtf8());
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
    return m_aspect == "16:9" || m_aspect == "16:10";
}

QString ThemeInfo::GetDirectoryName() const
{
#ifdef Q_OS_ANDROID
    return m_theme.fileName().remove("assets:/");
#else
    return m_theme.fileName();
#endif
}

void ThemeInfo::ToMap(InfoMap &infoMap) const
{
    infoMap["description"] = m_description;
    infoMap["name"] = m_name;
    infoMap["basetheme"] = m_baseTheme;
    infoMap["aspect"] = m_aspect;
    infoMap["resolution"] = QString("%1x%2").arg(m_baseres.width())
                                            .arg(m_baseres.height());
    infoMap["errata"] = m_errata;
    infoMap["majorversion"] = QString::number(m_majorver);
    infoMap["minorversion"] = QString::number(m_minorver);
    infoMap["version"] = QString("%1.%2").arg(m_majorver).arg(m_minorver);

    infoMap["authorname"] = m_authorName;
    infoMap["authoremail"] = m_authorEmail;
}
