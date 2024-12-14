
#include "mythuiclock.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QTimeZone>

#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

MythUIClock::MythUIClock(MythUIType *parent, const QString &name)
    : MythUIText(parent, name),
      m_timeFormat(GetMythDB()->GetSetting("TimeFormat", "h:mm ap")),
      m_dateFormat(GetMythDB()->GetSetting("DateFormat", "ddd d MMM yyyy")),
      m_shortDateFormat(GetMythDB()->GetSetting("ShortDateFormat", "ddd M/d"))
{
    m_format = QString("%1, %2").arg(m_dateFormat, m_timeFormat);
}

MythUIClock::~MythUIClock()
{
    delete m_font;
    m_font = nullptr;
}

/** \brief Looks up the time and sets the clock if the current time is
 *         greater than or equal to m_nextUpdate.
 */
void MythUIClock::Pulse(void)
{
    m_time = MythDate::current();

    if (m_nextUpdate.isNull() || (m_time >= m_nextUpdate))
        MythUIText::SetText(GetTimeText());

    MythUIText::Pulse();
}

/** \brief This creates a string based on m_time, and sets m_nextUpdate
 *         to the second following m_time.
 *
 *  It's important to note that this function do not look up the
 *  wall clock time, but depends on m_time being set ahead of time.
 *
 */
QString MythUIClock::GetTimeText(void)
{
    QDateTime dt = m_time.toLocalTime();
    QString newMsg = gCoreContext->GetQLocale().toString(dt, m_format);

    m_nextUpdate = m_time.addSecs(1);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    m_nextUpdate = QDateTime(m_time.date(),
                             m_time.time().addMSecs(m_time.time().msec()),
                             Qt::UTC);
#else
    m_nextUpdate = QDateTime(m_time.date(),
                             m_time.time().addMSecs(m_time.time().msec()),
                             QTimeZone(QTimeZone::UTC));
#endif

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
        m_time = MythDate::current();
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
        format = QCoreApplication::translate("ThemeUI", format.toUtf8());
        format.replace("%TIME%", m_timeFormat, Qt::CaseInsensitive);
        format.replace("%DATE%", m_dateFormat, Qt::CaseInsensitive);
        format.replace("%SHORTDATE%", m_shortDateFormat, Qt::CaseInsensitive);
        m_format = format;
        m_message = gCoreContext->GetQLocale().toString(QDateTime::currentDateTime(), m_format);
    }
    else
    {
        m_message = gCoreContext->GetQLocale().toString(QDateTime::currentDateTime(), m_format);
        return MythUIText::ParseElement(filename, element, showWarnings);
    }

    return true;
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIClock::CopyFrom(MythUIType *base)
{
    auto *clock = dynamic_cast<MythUIClock *>(base);

    if (!clock)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, bad parsing");
        return;
    }

    m_time = clock->m_time;
    m_nextUpdate = clock->m_nextUpdate;

    m_format = clock->m_format;
    m_timeFormat = clock->m_timeFormat;
    m_dateFormat = clock->m_dateFormat;
    m_shortDateFormat = clock->m_shortDateFormat;

    m_flash = clock->m_flash;

    MythUIText::CopyFrom(base);
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIClock::CreateCopy(MythUIType *parent)
{
    auto *clock = new MythUIClock(parent, objectName());
    clock->CopyFrom(this);
}
