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
class MythUIProgressDialog;

/** \class MythNews
 *  \brief Plugin for browsing RSS news feeds.
 */
class MythNews : public MythScreenType
{
    Q_OBJECT

  public:
    MythNews(MythScreenStack *parent, const QString &name);
    ~MythNews();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  private:
    void updateInfoView(void);
    void clearSites(void);
    void cancelRetrieve(void);
    void processAndShowNews(NewsSite *site);

    bool getHttpFile(const QString &sFilename, const QString &cmdURL);
    void createProgress(const QString &title);
    QString formatSize(long long bytes, int prec);
    void playVideo(const NewsArticle &article);

    // menu stuff
    void ShowMenu(void);
    void deleteNewsSite(void);
    void ShowEditDialog(bool edit);
    void ShowFeedManager();

    mutable QMutex m_lock;
    NewsSite::List m_NewsSites;

    QTimer        *m_RetrieveTimer;
    int            m_TimerTimeout;
    unsigned int   m_UpdateFreq;

    QString        m_zoom;
    QString        m_browser;
    MythDialogBox *m_menuPopup;

    MythUIProgressDialog *m_progressPopup;
    HttpComms            *m_httpGrabber;
    bool                  m_abortHttp;

    MythUIButtonList *m_sitesList;
    MythUIButtonList *m_articlesList;
    QMap<MythUIButtonListItem*,NewsArticle> m_articles;

    MythUIText *m_nositesText;
    MythUIText *m_updatedText;
    MythUIText *m_titleText;
    MythUIText *m_descText;

    MythUIImage *m_thumbnailImage;
    MythUIImage *m_downloadImage;
    MythUIImage *m_enclosureImage;
    MythUIImage *m_podcastImage;

  private slots:
    void loadSites(void);
    void updateInfoView(MythUIButtonListItem*);
    void slotViewArticle(MythUIButtonListItem*);
    void slotRetrieveNews(void);
    void slotNewsRetrieved(NewsSite*);
    void slotSiteSelected(MythUIButtonListItem*);

    void slotProgressCancelled();
};

#endif /* MYTHNEWS_H */
