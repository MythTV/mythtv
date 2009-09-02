#ifndef PROGDETAILS_H_
#define PROGDETAILS_H_

// qt
#include <QString>
#include <QKeyEvent>

// myth
#include "mythscreentype.h"
#include "mythuiwebbrowser.h"
#include "programinfo.h"

class ProgDetails : public MythScreenType
{
    Q_OBJECT
  public:
     ProgDetails(MythScreenStack *parent, const ProgramInfo *progInfo);
    ~ProgDetails();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);
    void Init(void);

  private:
    QString getRatings(bool recorded, uint chanid, QDateTime startts);
    void loadPage(void);
    void updatePage(void);
    bool loadHTML(void);
    void addItem(const QString &key, const QString &title, const QString &data);
    void removeItem(const QString &key);
    void showMenu(void);
    void customEvent(QEvent *event);

    ProgramInfo        m_progInfo;
    MythUIWebBrowser  *m_browser;

    int                m_currentPage;
    QString            m_page[2];

    QStringList        m_html;
};

#endif
