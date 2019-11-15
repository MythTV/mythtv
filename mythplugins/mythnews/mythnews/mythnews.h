#ifndef MYTHNEWS_H
#define MYTHNEWS_H

// MythTV headers
#include <mythscreentype.h>

// MythNews headers
#include "newssite.h"

class QTimer;
class HttpComms;
class MythUIText;
class MythUIImage;
class MythDialogBox;
class MythUIButtonList;
class MythUIButtonListItem;

/** \class MythNews
 *  \brief Plugin for browsing RSS news feeds.
 */
class MythNews : public MythScreenType
{
    Q_OBJECT

  public:
    MythNews(MythScreenStack *parent, const QString &name);
    ~MythNews();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void customEvent(QEvent*) override; // MythUIType

  private:
    void updateInfoView(void);
    void clearSites(void);
    void cancelRetrieve(void);
    void processAndShowNews(NewsSite *site);

    static QString formatSize(long long bytes, int prec);
    static void playVideo(const NewsArticle &article);

    // menu stuff
    void ShowMenu(void) override; // MythScreenType
    void deleteNewsSite(void);
    void ShowEditDialog(bool edit);
    void ShowFeedManager();

    mutable QMutex m_lock            {QMutex::Recursive};
    NewsSite::List m_NewsSites;

    QTimer        *m_RetrieveTimer   {nullptr};
    int            m_TimerTimeout    {10*60*1000};
    unsigned int   m_UpdateFreq      {30};

    QString        m_zoom            {"1.0"};
    QString        m_browser;
    MythDialogBox *m_menuPopup       {nullptr};

    MythUIButtonList *m_sitesList    {nullptr};
    MythUIButtonList *m_articlesList {nullptr};
    QMap<MythUIButtonListItem*,NewsArticle> m_articles;

    MythUIText *m_nositesText        {nullptr};
    MythUIText *m_updatedText        {nullptr};
    MythUIText *m_titleText          {nullptr};
    MythUIText *m_descText           {nullptr};

    MythUIImage *m_thumbnailImage    {nullptr};
    MythUIImage *m_downloadImage     {nullptr};
    MythUIImage *m_enclosureImage    {nullptr};
    MythUIImage *m_podcastImage      {nullptr};

  private slots:
    void loadSites(void);
    void updateInfoView(MythUIButtonListItem*);
    void slotViewArticle(MythUIButtonListItem*);
    void slotRetrieveNews(void);
    void slotNewsRetrieved(NewsSite*);
    void slotSiteSelected(MythUIButtonListItem*);
};

#endif /* MYTHNEWS_H */
