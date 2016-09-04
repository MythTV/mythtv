#ifndef PROGDETAILS_H_
#define PROGDETAILS_H_

// qt
#include <QString>
#include <QKeyEvent>

// myth
#include "mythscreentype.h"
#include "programinfo.h"
#include "proginfolist.h"

class ProgDetails : public MythScreenType
{
    Q_OBJECT
  public:
     ProgDetails(MythScreenStack *parent, const ProgramInfo *progInfo);
    ~ProgDetails();

    bool Create(void);
    void Init(void);
    bool keyPressEvent(QKeyEvent *event);

  private:
    QString getRatings(bool recorded, uint chanid, QDateTime startts);
    void updatePage(void);
    void addItem(const QString &title, const QString &value,
                 ProgInfoList::VisibleLevel level);
    void loadPage(void);

    ProgramInfo        m_progInfo;
    ProgInfoList       m_infoList;
    ProgInfoList::DataList m_data;
};

#endif
