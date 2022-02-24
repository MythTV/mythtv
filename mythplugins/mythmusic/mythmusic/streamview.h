#ifndef STREAMVIEW_H_
#define STREAMVIEW_H_

// qt
#include <QEvent>
#include <QTimer>
#include <QVector>

// MythTV
#include <libmythui/mythscreentype.h>

// mythmusic
#include "musiccommon.h"

class MythUIText;
class MythUIProgressBar;

class StreamView : public MusicCommon
{
  Q_OBJECT

  public:
    StreamView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~StreamView(void) override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MusicCommon

    void addStream(MusicMetadata *mdata);
    void deleteStream(MusicMetadata *mdata);
    void updateStream(MusicMetadata *mdata);

    void ShowMenu(void) override; // MusicCommon

  protected:
    void customEvent(QEvent *event) override; // MusicCommon
    void updateStreamList(void);
    void editStream(void);
    void removeStream(void);

  private slots:
    void streamItemClicked(MythUIButtonListItem *item);
    static void streamItemVisible(MythUIButtonListItem *item);
    void doRemoveStream(bool ok);

  private:
    MythUIButtonList  *m_streamList     {nullptr};
    MythUIText        *m_noStreams      {nullptr};
    MythUIText        *m_bufferStatus   {nullptr};
    MythUIProgressBar *m_bufferProgress {nullptr};

    MusicMetadata     *m_currStream     {nullptr};
    MusicMetadata     *m_lastStream     {nullptr};
};

class EditStreamMetadata : public MythScreenType
{
    Q_OBJECT

  public:
    EditStreamMetadata(MythScreenStack *parentStack, StreamView *parent,
                       MusicMetadata *mdata = nullptr)
        : MythScreenType(parentStack, "editstreampopup"),
          m_parent(parent), m_streamMeta(mdata) {}

    bool Create() override; // MythScreenType
    void changeStreamMetadata(MusicMetadata *mdata);

  private slots:
    void searchClicked(void);
    void saveClicked(void);

  private:
    StreamView     *m_parent          {nullptr};

    MusicMetadata  *m_streamMeta      {nullptr};

    MythUITextEdit *m_broadcasterEdit {nullptr};
    MythUITextEdit *m_channelEdit     {nullptr};
    MythUITextEdit *m_descEdit        {nullptr};
    MythUITextEdit *m_url1Edit        {nullptr};
    MythUITextEdit *m_url2Edit        {nullptr};
    MythUITextEdit *m_url3Edit        {nullptr};
    MythUITextEdit *m_url4Edit        {nullptr};
    MythUITextEdit *m_url5Edit        {nullptr};
    MythUITextEdit *m_logourlEdit     {nullptr};
    MythUITextEdit *m_formatEdit      {nullptr};
    MythUITextEdit *m_genreEdit       {nullptr};
    MythUITextEdit *m_countryEdit     {nullptr};
    MythUITextEdit *m_languageEdit    {nullptr};

    MythUIButton   *m_searchButton    {nullptr};
    MythUIButton   *m_cancelButton    {nullptr};
    MythUIButton   *m_saveButton      {nullptr};
};

class SearchStream : public MythScreenType
{
    Q_OBJECT

  public:
    SearchStream(MythScreenStack *parentStack, EditStreamMetadata *parent)
        : MythScreenType(parentStack, "searchstream"), m_parent(parent) {}

    bool Create() override; // MythScreenType

  private slots:
    void doneLoading(void);
    void updateStreams(void);
    void doUpdateStreams(void);
    void streamClicked(MythUIButtonListItem *item);
    static void streamVisible(MythUIButtonListItem *item);

  private:
    void Load(void) override; // MythScreenType
    static void loadStreams(void);
    void updateBroadcasters(void);
    void updateGenres(void);
    void updateCountries(void);
    void updateLanguages(void);

    EditStreamMetadata   *m_parent {nullptr};
    QList<MusicMetadata>  m_streams;
    QStringList  m_broadcasters;
    QStringList  m_genres;

    QString m_oldBroadcaster;
    QString m_oldGenre;
    QString m_oldChannel;
    QString m_oldCountry;
    QString m_oldLanguage;

    MythUIButtonList *m_broadcasterList {nullptr};
    MythUIButtonList *m_genreList       {nullptr};
    MythUIButtonList *m_countryList     {nullptr};
    MythUIButtonList *m_languageList    {nullptr};
    MythUITextEdit   *m_channelEdit     {nullptr};
    MythUIButtonList *m_streamList      {nullptr};
    MythUIText       *m_matchesText     {nullptr};

    QTimer m_updateTimer;
    bool m_updating                     {false};
};
#endif
