
// Own header
#include "themeinfo.h"

// QT headers
#include <QFile>
#include <QDir>
#include <QDomElement>

// MythTV headers
#include "mythverbose.h"
#include "xmlparsebase.h" // for VERBOSE_XML

#define LOC      QString("ThemeInfo: ")
#define LOC_ERR  QString("ThemeInfo, Error: ")
#define LOC_WARN QString("ThemeInfo, Warning: ")

ThemeInfo::ThemeInfo(QString theme)
{
    m_theme = new QFileInfo (theme);
    m_type = THEME_UNKN;
    m_baseres = QSize(800, 600);
    m_majorver = m_minorver = 0;

    if (!parseThemeInfo())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("The theme (%1) is missing a themeinfo.xml file.")
                .arg(m_theme->fileName()));
    }
}

ThemeInfo::~ThemeInfo()
{
    delete m_theme;
}

bool ThemeInfo::parseThemeInfo()
{

    QDomDocument doc;

    QFile f(m_theme->absoluteFilePath() + "/themeinfo.xml");

    if (!f.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Unable to open themeinfo.xml "
                        "for %1").arg(m_theme->absoluteFilePath()));
        return false;
    }

    if (!doc.setContent(&f))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unable to parse themeinfo.xml "
                        "for %1").arg(m_theme->fileName()));

        f.close();
        return false;
    }
    f.close();

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
                                m_previewpath = m_theme->absoluteFilePath() + '/'
                                                    + thumbnail;
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
