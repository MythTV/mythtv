#ifndef STATUSBOX_H_
#define STATUSBOX_H_

#include <vector> // For std::vector

using namespace std;

#include "mythscreentype.h"

class ProgramInfo;
class MythUIText;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUIStateType;

typedef QMap<QString, unsigned int> recprof2bps_t;

class StatusBox : public MythScreenType
{
    Q_OBJECT
  public:
    StatusBox(MythScreenStack *parent);
   ~StatusBox(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  signals:
    void updateLog();

  protected:
    virtual void Init(void);
    
  private slots:
    void setHelpText(MythUIButtonListItem *item);
    void updateLogList(MythUIButtonListItem *item);
    void clicked(MythUIButtonListItem *item);

    void doListingsStatus();
    void doScheduleStatus();
    void doTunerStatus();
    void doLogEntries();
    void doJobQueueStatus();
    void doMachineStatus();
    void doAutoExpireList();

  private:
    MythUIButtonListItem* AddLogLine(const QString & line,
				     const QString & help = "",
				     const QString & detail = "",
				     const QString & helpdetail = "",
				     const QString & state = "",
				     const QString & data = "");

    void getActualRecordedBPS(QString hostnames);

    MythUIText *m_helpText;
    MythUIText *m_justHelpText;
    MythUIButtonList *m_categoryList;
    MythUIButtonList *m_logList;
    MythUIStateType *m_iconState;

    QString m_dateFormat, m_timeFormat, m_timeDateFormat;

    QMap<int, QString> contentData;
    recprof2bps_t      recordingProfilesBPS;

    vector<ProgramInfo *> m_expList;

    MythScreenStack *m_popupStack;

    int m_minLevel;

    bool m_isBackendActive;
};

#endif
