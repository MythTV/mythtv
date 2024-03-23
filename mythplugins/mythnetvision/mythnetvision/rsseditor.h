#ifndef RSSEDITOR_H
#define RSSEDITOR_H

#include <utility>

// Qt headers
#include <QDomDocument>
#include <QRecursiveMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

// MythTV headers
#include <libmythbase/rssmanager.h>
#include <libmythui/mythscreentype.h>

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
    /** \brief Creates a new RSS Edit Popup
     *  \param url The web page for which an entry is being created.
     *  \param edit If true, then editing an existing entry instead of
     *              creating a new entry.
     *  \param parent Pointer to the screen stack
     *  \param name The name of the window
     */
    RSSEditPopup(QString url, bool edit, MythScreenStack *parent,
                 const QString &name = "RSSEditPopup")
        : MythScreenType(parent, name),
          m_urlText(std::move(url)), m_editing(edit) {}
   ~RSSEditPopup() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    static QUrl redirectUrl(const QUrl& possibleRedirectUrl,
                            const QUrl& oldRedirectUrl) ;

    RSSSite                *m_site         {nullptr};
    QString                 m_urlText;
    bool                    m_editing;

    MythUIImage            *m_thumbImage   {nullptr};
    MythUIButton           *m_thumbButton  {nullptr};
    MythUITextEdit         *m_urlEdit      {nullptr};
    MythUITextEdit         *m_titleEdit    {nullptr};
    MythUITextEdit         *m_descEdit     {nullptr};
    MythUITextEdit         *m_authorEdit   {nullptr};

    MythUIButton           *m_okButton     {nullptr};
    MythUIButton           *m_cancelButton {nullptr};

    MythUICheckBox         *m_download     {nullptr};

    QNetworkAccessManager  *m_manager      {nullptr};
    QNetworkReply          *m_reply        {nullptr};

  signals:
    void Saving(void);

  private slots:
    void SlotCheckRedirect(QNetworkReply* reply);
    void ParseAndSave(void);
    void SlotSave(QNetworkReply *reply);
    void DoFileBrowser(void);
    static void SelectImagePopup(const QString &prefix,
                        QObject &inst,
                        const QString &returnEvent);
    void customEvent(QEvent *levent) override; // MythUIType
};

class RSSEditor : public MythScreenType
{
    Q_OBJECT

  public:
    explicit RSSEditor(MythScreenStack *parent, const QString &name = "RSSEditor")
        : MythScreenType(parent, name) {}
   ~RSSEditor() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    void fillRSSButtonList();
    mutable QRecursiveMutex  m_lock;
    bool              m_changed {false};

    RSSSite::rssList  m_siteList;
    MythUIButtonList *m_sites   {nullptr};
    MythUIButton     *m_new     {nullptr};
    MythUIButton     *m_delete  {nullptr};
    MythUIButton     *m_edit    {nullptr};

    MythUIImage      *m_image   {nullptr};
    MythUIText       *m_title   {nullptr};
    MythUIText       *m_url     {nullptr};
    MythUIText       *m_desc    {nullptr};
    MythUIText       *m_author  {nullptr};

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
