#ifndef MYTHUI_CLOCK_H_
#define MYTHUI_CLOCK_H_

#include <QString>
#include <QDateTime>

#include "mythuitype.h"
#include "mythuitext.h"

/** \class MythUIClock
 *
 * \brief A simple text clock widget.
 *
 * Updates once a second and inherits from MythUIText, so it supports all the
 * text styles and decorations offered by that class.
 *
 * Basic manipulation of the clock format is supported using any of the
 * following in the "format" element of the theme:
 *  %TIME% - The time, in a format defined in the locale settings
 *  %DATE% - Long date format, as defined in the locale settings
 *  %SHORTDATE% - Short date format, as defined in the locale settings
 *
 * \ingroup MythUI_Widgets
 */
class MPUBLIC MythUIClock : public MythUIText
{
  public:
    MythUIClock(MythUIType *parent, const QString &name);
   ~MythUIClock();

    virtual void Pulse(void);
    virtual void SetText(const QString &text);

  protected:
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    QString GetTimeText(void);

    QDateTime m_Time;
    QDateTime m_nextUpdate;

    QString m_Format;
    QString m_TimeFormat;
    QString m_DateFormat;
    QString m_ShortDateFormat;

    bool m_Flash;
};

#endif
