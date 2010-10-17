
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
    m_Message = m_Time.toString(m_Format);

    m_DateFormat = GetMythDB()->GetSetting("DateFormat", "ddd d MMMM");
    m_ShortDateFormat = GetMythDB()->GetSetting("ShortDateFormat", "ddd d");
    m_TimeFormat = GetMythDB()->GetSetting("TimeFormat", "hh:mm");

    m_Format = QString("%1, %2").arg(m_DateFormat).arg(m_TimeFormat);

    m_Flash = false;

    MythUIText::SetText(GetTimeText());
}

MythUIClock::~MythUIClock()
{
    delete m_Font;
    m_Font = NULL;
}

/** \brief Looks up the time and sets the clock if the current time is
 *         greater than or equal to m_nextUpdate.
 */
void MythUIClock::Pulse(void)
{
    m_Time = QDateTime::currentDateTime();
    if (m_nextUpdate.isNull() || (m_Time >= m_nextUpdate))
        MythUIText::SetText(GetTimeText());

    MythUIText::Pulse();
}

/** \brief This creates a string based on m_Time, and sets m_nextUpdate
 *         to the second following m_Time.
 *
 *  It's important to note that this function do not look up the
 *  wall clock time, but depends on m_Time being set ahead of time.
 *
 */
QString MythUIClock::GetTimeText(void)
{
    QString newMsg = m_Time.toString(m_Format);

    m_nextUpdate = m_Time.addSecs(1);
    m_nextUpdate = QDateTime(
        m_Time.date(), m_Time.time().addMSecs(m_Time.time().msec()));

    return newMsg;
}

/** \brief This sets the text, unless the string is blank, in that
 *         case the time is looked up and set as the text instead.
 */
void MythUIClock::SetText(const QString &text)
{
    QString txt = text;
    if (txt.isEmpty())
    {
        m_Time = QDateTime::currentDateTime();
        txt = GetTimeText();
    }
    MythUIText::SetText(txt);
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

