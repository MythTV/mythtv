#ifndef PROGDETAILS_H_
#define PROGDETAILS_H_

// qt
#include <QString>
#include <QKeyEvent>

// myth
#include "mythscreentype.h"
#include "mythuiwebbrowser.h"

class ProgDetails : public MythScreenType
{
    Q_OBJECT
  public:
      ProgDetails(MythScreenStack *parent, const char *name);
    ~ProgDetails();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);

    QString themeText(const QString &fontName, const QString &text, int size);
    void setDetails(const QString &details);

  private:

    MythUIWebBrowser *m_browser;
    QString           m_details;
};

#endif
