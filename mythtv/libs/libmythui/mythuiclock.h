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
class MUI_PUBLIC MythUIClock : public MythUIText
{
  public:
    MythUIClock(MythUIType *parent, const QString &name);
   ~MythUIClock() override;

    void Pulse(void) override; // MythUIText
    void SetText(const QString &text) override; // MythUIText

  protected:
    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIText
    void CopyFrom(MythUIType *base) override; // MythUIText
    void CreateCopy(MythUIType *parent) override; // MythUIText

    QString GetTimeText(void);

    QDateTime m_time;
    QDateTime m_nextUpdate;

    QString m_format;
    QString m_timeFormat      {"h:mm ap"};
    QString m_dateFormat      {"ddd d MMM yyyy"};
    QString m_shortDateFormat {"ddd M/d"};

    bool    m_flash           {false};
};

#endif
