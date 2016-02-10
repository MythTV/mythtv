#ifndef STREAMVIEW_H_
#define STREAMVIEW_H_

// qt
#include <QEvent>
#include <QVector>
#include <QTimer>

// myth
#include <mythscreentype.h>

// mythmusic
#include <musiccommon.h>

class MythUIWebBrowser;
class MythUIText;
class MythUIProgressBar;

class StreamView : public MusicCommon
{
  Q_OBJECT

  public:
    StreamView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~StreamView(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void addStream(MusicMetadata *mdata);
    void deleteStream(MusicMetadata *mdata);
    void updateStream(MusicMetadata *mdata);

    virtual void ShowMenu(void);

  protected:
    void customEvent(QEvent *event);
    void updateStreamList(void);
    void editStream(void);
    void removeStream(void);

  private slots:
    void streamItemClicked(MythUIButtonListItem *item);
    void streamItemVisible(MythUIButtonListItem *item);
    void doRemoveStream(bool ok);

  private:
    MythUIButtonList  *m_streamList;
    MythUIText        *m_noStreams;
    MythUIText        *m_bufferStatus;
    MythUIProgressBar *m_bufferProgress;
};

class EditStreamMetadata : public MythScreenType
{
    Q_OBJECT

  public:
    EditStreamMetadata(MythScreenStack *parentStack, StreamView *parent,
                       MusicMetadata *mdata = NULL);

    bool Create();
    void changeStreamMetadata(MusicMetadata *mdata);

  private slots:
    void searchClicked(void);
    void saveClicked(void);

  private:
    StreamView     *m_parent;

    MusicMetadata  *m_streamMeta;

    MythUITextEdit *m_broadcasterEdit;
    MythUITextEdit *m_channelEdit;
    MythUITextEdit *m_descEdit;
    MythUITextEdit *m_url1Edit;
    MythUITextEdit *m_url2Edit;
    MythUITextEdit *m_url3Edit;
    MythUITextEdit *m_url4Edit;
    MythUITextEdit *m_url5Edit;
    MythUITextEdit *m_logourlEdit;
    MythUITextEdit *m_formatEdit;
    MythUITextEdit *m_genreEdit;
    MythUITextEdit *m_countryEdit;
    MythUITextEdit *m_languageEdit;

    MythUIButton   *m_searchButton;
    MythUIButton   *m_cancelButton;
    MythUIButton   *m_saveButton;
};

class SearchStream : public MythScreenType
{
    Q_OBJECT

  public:
    SearchStream(MythScreenStack *parentStack, EditStreamMetadata *parent);

    bool Create();

  private slots:
    void doneLoading(void);
    void updateStreams(void);
    void doUpdateStreams(void);
    void streamClicked(MythUIButtonListItem *item);
    void streamVisible(MythUIButtonListItem *item);

  private:
    void Load(void);
    void loadStreams(void);
    void updateBroadcasters(void);
    void updateGenres(void);
    void updateCountries(void);
    void updateLanguages(void);

    EditStreamMetadata   *m_parent;
    QList<MusicMetadata>  m_streams;
    QStringList  m_broadcasters;
    QStringList  m_genres;

    QString m_oldBroadcaster;
    QString m_oldGenre;
    QString m_oldChannel;
    QString m_oldCountry;
    QString m_oldLanguage;

    MythUIButtonList *m_broadcasterList;
    MythUIButtonList *m_genreList;
    MythUIButtonList *m_countryList;
    MythUIButtonList *m_languageList;
    MythUITextEdit   *m_channelEdit;
    MythUIButtonList *m_streamList;
    MythUIText       *m_matchesText;

    QTimer m_updateTimer;
    bool m_updating;
};
#endif
