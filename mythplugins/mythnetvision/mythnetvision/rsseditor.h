#ifndef RSSEDITOR_H
#define RSSEDITOR_H

// Qt headers
#include <QMutex>
#include <QString>
#include <QDomDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>

// MythTV headers
#include <mythscreentype.h>

#include "mythrssmanager.h"

class MythUITextEdit;
class MythUIButton;
class MythUICheckBox;
class RSSSite;

/** \class RSSEditPopup
 *  \brief Site name, URL and icon edit screen.
 */
class RSSEditPopup : public MythScreenType
{
    Q_OBJECT

  public:
    RSSEditPopup(const QString &url, bool edit, MythScreenStack *parent,
                 const QString &name = "RSSEditPopup");
   ~RSSEditPopup();

    bool Create(void);
    bool keyPressEvent(QKeyEvent*);

  private:
    QUrl redirectUrl(const QUrl& possibleRedirectUrl,
                     const QUrl& oldRedirectUrl) const;

    RSSSite                *m_site;
    QString                 m_urlText;
    bool                    m_editing;

    MythUIImage            *m_thumbImage;
    MythUIButton           *m_thumbButton;
    MythUITextEdit         *m_urlEdit;
    MythUITextEdit         *m_titleEdit;
    MythUITextEdit         *m_descEdit;
    MythUITextEdit         *m_authorEdit;

    MythUIButton           *m_okButton;
    MythUIButton           *m_cancelButton;

    MythUICheckBox         *m_download;

    QNetworkAccessManager  *m_manager;
    QNetworkReply          *m_reply;

  signals:
    void Saving(void);

  private slots:
    void SlotCheckRedirect(QNetworkReply* reply);
    void ParseAndSave(void);
    void SlotSave(QNetworkReply *reply);
    void DoFileBrowser(void);
    void SelectImagePopup(const QString &prefix,
                        QObject &inst,
                        const QString &returnEvent);
    void customEvent(QEvent *levent);
};

class RSSEditor : public MythScreenType
{
    Q_OBJECT

  public:
    RSSEditor(MythScreenStack *parent, const QString &name = "RSSEditor");
   ~RSSEditor();

    bool Create(void);
    bool keyPressEvent(QKeyEvent*);

  private:
    void fillRSSButtonList();
    mutable QMutex  m_lock;
    bool m_changed;

    RSSSite::rssList m_siteList;
    MythUIButtonList *m_sites;
    MythUIButton     *m_new;
    MythUIButton     *m_delete;
    MythUIButton     *m_edit;

    MythUIImage      *m_image;
    MythUIText       *m_title;
    MythUIText       *m_url;
    MythUIText       *m_desc;
    MythUIText       *m_author;

  signals:
    void ItemsChanged(void);

  public slots:
    void SlotItemChanged();
    void LoadData(void);

    void SlotDeleteSite(void);
    void DoDeleteSite(bool remove);
    void SlotEditSite(void);
    void SlotNewSite(void);
    void ListChanged(void);
};

#endif /* RSSEDITOR_H */
