#ifndef STATUSBOX_H_
#define STATUSBOX_H_

// Qt
#include <QTimer>

// MythTV
#include "mythscreentype.h"
#include "mythuibuttonlist.h"

// Std
#include <vector> // For std::vector
using namespace std;

class ProgramInfo;
class MythUIText;
class MythUIButtonList;

class MythUIStateType;

using recprof2bps_t = QMap<QString, unsigned int>;

class StatusBoxItem : public QTimer, public MythUIButtonListItem
{
    Q_OBJECT

  public:
    StatusBoxItem(MythUIButtonList *lbtype, const QString& text, QVariant data)
      : MythUIButtonListItem (lbtype, text, data) { }

    void Start(int Interval = 1); // Seconds

  signals:
    void UpdateRequired(StatusBoxItem* Item);
};

class StatusBox : public MythScreenType
{
    Q_OBJECT
  public:
    explicit StatusBox(MythScreenStack *parent);
   ~StatusBox(void) override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

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
    void doDisplayStatus();
    void doDecoderStatus();

  private:
    StatusBoxItem* AddLogLine(const QString & line,
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

    recprof2bps_t      m_recordingProfilesBps;

    vector<ProgramInfo *> m_expList;

    MythScreenStack   *m_popupStack      {nullptr};

    int                m_minLevel        {5};

    bool               m_isBackendActive {false};
};

#endif
