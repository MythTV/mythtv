#ifndef VIDEOSETUPWIZARD_H
#define VIDEOSETUPWIZARD_H

// MythTV
#include "libmythtv/mythvideoprofile.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythprogressdialog.h"
#include "libmythui/mythscreentype.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"

extern const QString VIDEO_SAMPLE_HD_LOCATION;
extern const QString VIDEO_SAMPLE_SD_LOCATION;
extern const QString VIDEO_SAMPLE_HD_FILENAME;
extern const QString VIDEO_SAMPLE_SD_FILENAME;

class VideoSetupWizard : public MythScreenType
{
  Q_OBJECT

  public:

    VideoSetupWizard(MythScreenStack *parent, MythScreenType *general,
                     MythScreenType *audio, const char *name = nullptr);
    ~VideoSetupWizard() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *e) override; // MythUIType

    void save(void);

  private:
    void initProgressDialog();

    enum TestType
    {
        ttNone = 0,
        ttHighDefinition,
        ttStandardDefinition
    };

    QString              m_downloadFile;
    TestType             m_testType                   {ttNone};

    MythScreenType      *m_generalScreen              {nullptr};
    MythScreenType      *m_audioScreen                {nullptr};

    MythUIButtonList     *m_playbackProfileButtonList {nullptr};
    MythScreenStack      *m_popupStack                {nullptr};
    MythUIProgressDialog *m_progressDialog            {nullptr};

    MythUIButton        *m_testSDButton               {nullptr};
    MythUIButton        *m_testHDButton               {nullptr};

    MythUIButton        *m_nextButton                 {nullptr};
    MythUIButton        *m_prevButton                 {nullptr};

  private slots:
    void slotNext(void);
    void slotPrevious(void);
    void loadData(void);

    void testSDVideo(void);
    void testHDVideo(void);
    void playVideoTest(const QString& desc,
                       const QString& title,
                       const QString& file);

    void DownloadSample(const QString& url, const QString& dest);
};

#endif
