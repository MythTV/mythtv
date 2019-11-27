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

using recprof2bps_t = QMap<QString, unsigned int>;

class StatusBox : public MythScreenType
{
    Q_OBJECT
  public:
    explicit StatusBox(MythScreenStack *parent);
   ~StatusBox(void);

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void customEvent(QEvent*) override; // MythUIType

  signals:
    void updateLog();

  protected:
    void Init(void) override; // MythScreenType
    
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
    void doAutoExpireList(bool updateExpList = true);

  private:
    void AddLogLine(const QString & line,
                     const QString & help = "",
                     const QString & detail = "",
                     const QString & helpdetail = "",
                     const QString & state = "",
                     const QString & data = "");

    void getActualRecordedBPS(const QString& hostnames);

    MythUIText        *m_helpText        {nullptr};
    MythUIText        *m_justHelpText    {nullptr};
    MythUIButtonList  *m_categoryList    {nullptr};
    MythUIButtonList  *m_logList         {nullptr};
    MythUIStateType   *m_iconState       {nullptr};

    QMap<int, QString> contentData;
    recprof2bps_t      recordingProfilesBPS;

    vector<ProgramInfo *> m_expList;

    MythScreenStack   *m_popupStack      {nullptr};

    int                m_minLevel        {5};

    bool               m_isBackendActive {false};
};

#endif
