
// Own header
#include "themeinfo.h"

// QT headers
#include <QFile>
#include <QDir>
#include <QDomElement>

// MythTV headers
#include "mythverbose.h"
#include "xmlparsebase.h" // for VERBOSE_XML
#include "mythdownloadmanager.h"

#define LOC      QString("ThemeInfo: ")
#define LOC_ERR  QString("ThemeInfo, Error: ")
#define LOC_WARN QString("ThemeInfo, Warning: ")

ThemeInfo::ThemeInfo(QString theme)
{
    m_theme = new QFileInfo (theme);
    m_type = THEME_UNKN;
    m_baseres = QSize(800, 600);
    m_majorver = m_minorver = 0;

    if (m_theme->exists())
        m_themeurl = m_theme->absoluteFilePath();
    else
        m_themeurl = theme;

    if (!parseThemeInfo())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("The theme (%1) is missing a themeinfo.xml file.")
                .arg(m_themeurl));
    }
}

ThemeInfo::~ThemeInfo()
{
    delete m_theme;
}

bool ThemeInfo::parseThemeInfo()
{

    QDomDocument doc;

    if ((m_themeurl.startsWith("http://")) ||
        (m_themeurl.startsWith("https://")) ||
        (m_themeurl.startsWith("ftp://")))
    {
        QByteArray data;
        bool ok = GetMythDownloadManager()->download(m_themeurl +
                                                     "/themeinfo.xml", &data);
        if (!ok)
            return false;

        if (!doc.setContent(data))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Unable to parse themeinfo.xml "
                            "for %1").arg(m_themeurl));
            return false;
        }
    }
    else
    {
        QFile f(m_themeurl + "/themeinfo.xml");

        if (!f.open(QIODevice::ReadOnly))
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    QString("Unable to open themeinfo.xml "
                            "for %1").arg(f.fileName()));
            return false;
        }

        if (!doc.setContent(&f))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Unable to parse themeinfo.xml "
                            "for %1").arg(f.fileName()));

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
                m_name = e.firstChild().toText().data();
            }
            else if (e.tagName() == "aspect")
            {
                m_aspect = e.firstChild().toText().data();
            }
            else if (e.tagName() == "baseres")
            {
                    QString size = e.firstChild().toText().data();
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
                            QString type = ce.firstChild().toText().data();

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
                                VERBOSE_XML(VB_IMPORTANT,
                                            m_theme->fileName(),
                                            ce, LOC_ERR + "Invalid theme type");
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
                            m_majorver = ce.firstChild().toText()
                                                    .data().toInt();
                        }
                        else if (ce.tagName() == "minor")
                        {
                            m_minorver = ce.firstChild().toText()
                                                    .data().toInt();
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
                            m_authorName = ce.firstChild().toText().data();
                        }
                        else if (ce.tagName() == "email")
                        {
                            m_authorEmail = ce.firstChild().toText().data();
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
                                QString thumbnail = ce.firstChild().toText()
                                                                    .data();
                                m_previewpath = m_themeurl + '/' + thumbnail;
                            }
                        }
                        else if (ce.tagName() == "description")
                        {
                            m_description = ce.firstChild().toText().data();
                        }
                        else if (ce.tagName() == "errata")
                        {
                            m_errata = ce.firstChild().toText().data();
                        }
                        else if (ce.tagName() == "downloadurl")
                        {
                            m_downloadurl = ce.firstChild().toText().data();
                        }
                        else if (ce.tagName() == "themesite")
                        {
                            m_themesite = ce.firstChild().toText().data();
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

void ThemeInfo::ToMap(QHash<QString, QString> &infoMap) const
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
