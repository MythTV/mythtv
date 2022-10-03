#ifndef PROGDETAILS_H_
#define PROGDETAILS_H_

// qt
#include <QString>
#include <QKeyEvent>

// MythTV
#include "libmythbase/programinfo.h"
#include "libmythui/mythscreentype.h"

//  MythFrontend
#include "proginfolist.h"

class ProgDetails : public MythScreenType
{
    Q_OBJECT
  public:
     ProgDetails(MythScreenStack *parent, const ProgramInfo *progInfo)
         : MythScreenType (parent, "progdetails"),
           m_progInfo(*progInfo), m_infoList(*this) {}
    ~ProgDetails() override;

    bool Create(void) override; // MythScreenType
    void Init(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    static QString getRatings(bool recorded, uint chanid, const QDateTime& startts);
    void updatePage(void);
    void addItem(const QString &title, const QString &value,
                 ProgInfoList::VisibleLevel level);
    void PowerPriorities(const QString & ptable);
    void loadPage(void);

    ProgramInfo        m_progInfo;
    ProgInfoList       m_infoList;
    ProgInfoList::DataList m_data;
};

#endif
