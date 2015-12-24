#ifndef LYRICSVIEW_H_
#define LYRICSVIEW_H_

// qt
#include <QEvent>
#include <QVector>

// myth
#include <mythscreentype.h>
#include <musicmetadata.h>
#include <lyricsdata.h>

// mythmusic
#include <musiccommon.h>


class LyricsView : public MusicCommon
{
  Q_OBJECT

  public:
    LyricsView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~LyricsView(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    virtual void ShowMenu(void);

  protected:
    void customEvent(QEvent *event);

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
    MythUIButtonList  *m_lyricsList;
    MythUIText        *m_statusText;
    MythUIStateType   *m_loadingState;
    MythUIText        *m_bufferStatus;
    MythUIProgressBar *m_bufferProgress;

    LyricsData       *m_lyricData;

    bool              m_autoScroll;
};

class EditLyricsDialog : public MythScreenType
{

  Q_OBJECT

  public:

    EditLyricsDialog(MythScreenStack *parent, LyricsData *sourceData);
    ~EditLyricsDialog();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *e);

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

    LyricsData     *m_sourceData;

    MythUITextEdit *m_grabberEdit;
    MythUICheckBox *m_syncronizedCheck;
    MythUITextEdit *m_titleEdit;
    MythUITextEdit *m_artistEdit;
    MythUITextEdit *m_albumEdit;
    MythUITextEdit *m_lyricsEdit;
    MythUIButton   *m_cancelButton;
    MythUIButton   *m_okButton;
};

#endif
