#ifndef MYTHNEWS_H
#define MYTHNEWS_H

// MythTV headers
#include <libmythui/mythscreentype.h>

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
    ~MythNews() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

  private:
    void updateInfoView(void);
    void clearSites(void);
    void cancelRetrieve(void);
    void processAndShowNews(NewsSite *site);
    static QString cleanText(const QString &text);

    static void playVideo(const NewsArticle &article);

    // menu stuff
    void ShowMenu(void) override; // MythScreenType
    void deleteNewsSite(void);
    void ShowEditDialog(bool edit);
    void ShowFeedManager() const;

    mutable QRecursiveMutex m_lock;
    NewsSite::List m_newsSites;

    QTimer        *m_retrieveTimer   {nullptr};
    std::chrono::minutes  m_timerTimeout  {10min};
    std::chrono::minutes  m_updateFreq    {30min};

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
    void updateInfoView(MythUIButtonListItem *selected);
    void slotViewArticle(MythUIButtonListItem *articlesListItem);
    void slotRetrieveNews(void);
    void slotNewsRetrieved(NewsSite *site);
    void slotSiteSelected(MythUIButtonListItem *item);
};

#endif /* MYTHNEWS_H */
