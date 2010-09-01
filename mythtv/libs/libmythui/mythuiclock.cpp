
#include "mythuiclock.h"

#include <QCoreApplication>
#include <QDomDocument>

#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

#include "mythverbose.h"
#include "mythdb.h"

MythUIClock::MythUIClock(MythUIType *parent, const QString &name)
           : MythUIText(parent, name)
{
    m_Time = QDateTime::currentDateTime();
    m_nextUpdate = m_Time.addSecs(1);
    m_Message = m_Time.toString(m_Format);

    m_DateFormat = GetMythDB()->GetSetting("DateFormat", "ddd d MMMM");
    m_ShortDateFormat = GetMythDB()->GetSetting("ShortDateFormat", "ddd d");
    m_TimeFormat = GetMythDB()->GetSetting("TimeFormat", "hh:mm");

    m_Format = QString("%1, %2").arg(m_DateFormat).arg(m_TimeFormat);

    m_Flash = false;
}

MythUIClock::~MythUIClock()
{
    delete m_Font;
    m_Font = NULL;
}

/**
 *  \copydoc MythUIType::Pulse()
 */
void MythUIClock::Pulse(void)
{
    m_Time = QDateTime::currentDateTime();

    if (m_Time > m_nextUpdate)
    {
        QString newMsg;

        newMsg = m_Time.toString(m_Format);

        SetText(newMsg);

        m_nextUpdate = m_Time.addSecs(1);
    }

    MythUIText::Pulse();
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIClock::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "format" ||
        element.tagName() == "template")
    {
        QString format = parseText(element);
        format.replace("%TIME%", m_TimeFormat, Qt::CaseInsensitive);
        format.replace("%DATE%", m_DateFormat, Qt::CaseInsensitive);
        format.replace("%SHORTDATE%", m_ShortDateFormat, Qt::CaseInsensitive);
        m_Format = format;
    }
    else
    {
        return MythUIText::ParseElement(filename, element, showWarnings);
    }

    return true;
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIClock::CopyFrom(MythUIType *base)
{
    MythUIClock *clock = dynamic_cast<MythUIClock *>(base);
    if (!clock)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_Time = clock->m_Time;
    m_nextUpdate = clock->m_nextUpdate;

    m_Format = clock->m_Format;
    m_TimeFormat = clock->m_TimeFormat;
    m_DateFormat = clock->m_DateFormat;
    m_ShortDateFormat = clock->m_ShortDateFormat;

    m_Flash = clock->m_Flash;

    MythUIText::CopyFrom(base);
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIClock::CreateCopy(MythUIType *parent)
{
    MythUIClock *clock = new MythUIClock(parent, objectName());
    clock->CopyFrom(this);
}

