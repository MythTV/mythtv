#ifndef LYRICSVIEW_H_
#define LYRICSVIEW_H_

// qt
#include <QEvent>
#include <QVector>

// myth
#include <libmythmetadata/lyricsdata.h>
#include <libmythmetadata/musicmetadata.h>
#include <libmythui/mythscreentype.h>

// mythmusic
#include "musiccommon.h"


class LyricsView : public MusicCommon
{
  Q_OBJECT

  public:
    LyricsView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~LyricsView(void) override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MusicCommon

    void ShowMenu(void) override; // MusicCommon

  protected:
    void customEvent(QEvent *event) override; // MusicCommon

  private:
    void findLyrics(const QString &grabber="ALL");
    void showLyrics(void);
    void saveLyrics(void);
    void editLyrics(void);
    void showMessage(const QString &message);
    MythMenu* createFindLyricsMenu(void);

  private slots:
    void setLyricTime(void);
    void editFinished(bool result);
    void lyricStatusChanged(LyricsData::Status status, const QString &message);

  private:
    MythUIButtonList  *m_lyricsList     {nullptr};
    MythUIText        *m_statusText     {nullptr};
    MythUIStateType   *m_loadingState   {nullptr};
    MythUIText        *m_bufferStatus   {nullptr};
    MythUIProgressBar *m_bufferProgress {nullptr};

    LyricsData        *m_lyricData      {nullptr};

    bool               m_autoScroll     {true};
};

class EditLyricsDialog : public MythScreenType
{

  Q_OBJECT

  public:

    EditLyricsDialog(MythScreenStack *parent, LyricsData *sourceData);
    ~EditLyricsDialog() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

  signals:
    void haveResult(bool ok);

  public slots:
    void okPressed(void);
    void cancelPressed(void);
    void saveEdits(bool ok);
    void syncronizedChanged(bool syncronized);

  private:
    void loadLyrics(void);
    bool somethingChanged(void);

    LyricsData     *m_sourceData       {nullptr};

    MythUITextEdit *m_grabberEdit      {nullptr};
    MythUICheckBox *m_syncronizedCheck {nullptr};
    MythUITextEdit *m_titleEdit        {nullptr};
    MythUITextEdit *m_artistEdit       {nullptr};
    MythUITextEdit *m_albumEdit        {nullptr};
    MythUITextEdit *m_lyricsEdit       {nullptr};
    MythUIButton   *m_cancelButton     {nullptr};
    MythUIButton   *m_okButton         {nullptr};
};

#endif
