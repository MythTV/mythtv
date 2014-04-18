#ifndef STREAMVIEW_H_
#define STREAMVIEW_H_

// qt
#include <QEvent>
#include <QVector>

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

    MythUITextEdit *m_stationEdit;
    MythUITextEdit *m_channelEdit;
    MythUITextEdit *m_urlEdit;
    MythUITextEdit *m_logourlEdit;
    MythUITextEdit *m_formatEdit;
    MythUITextEdit *m_genreEdit;

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
    void updateStreams(void);
    void streamClicked(MythUIButtonListItem *item);
    void streamVisible(MythUIButtonListItem *item);

  private:
    void loadStreams(void);
    void updateStations(void);
    void updateGenres(void);

    EditStreamMetadata      *m_parent;
    QMap<QString, MusicMetadata>  m_streams;
    QStringList  m_stations;
    QStringList  m_genres;

    MythUIButtonList *m_stationList;
    MythUIButtonList *m_genreList;
    MythUITextEdit   *m_channelEdit;
    MythUIButtonList *m_streamList;
    MythUIText       *m_matchesText;
};
#endif
