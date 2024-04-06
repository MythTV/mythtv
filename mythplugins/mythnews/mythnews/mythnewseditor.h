#ifndef MYTHNEWSEDITOR_H
#define MYTHNEWSEDITOR_H

// Qt headers
#include <QtGlobal>
#include <QRecursiveMutex>
#include <QString>

// MythTV headers
#include <libmythui/mythscreentype.h>

class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythUICheckBox;
class NewsSite;

/** \class MythNewsEdit
 *  \brief Site name, URL and icon edit screen.
 */
class MythNewsEditor : public MythScreenType
{
    Q_OBJECT

  public:
    MythNewsEditor(NewsSite *site, bool edit, MythScreenStack *parent,
                   const QString &name = "MythNewsEditor");
   ~MythNewsEditor() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    mutable QRecursiveMutex  m_lock;
    NewsSite       *m_site             {nullptr};
    QString         m_siteName;
    bool            m_editing;

    MythUIText     *m_titleText        {nullptr};
    MythUIText     *m_nameLabelText    {nullptr};
    MythUIText     *m_urlLabelText     {nullptr};
    MythUIText     *m_iconLabelText    {nullptr};
    MythUIText     *m_podcastLabelText {nullptr};

    MythUITextEdit *m_nameEdit         {nullptr};
    MythUITextEdit *m_urlEdit          {nullptr};
    MythUITextEdit *m_iconEdit         {nullptr};

    MythUIButton   *m_okButton         {nullptr};
    MythUIButton   *m_cancelButton     {nullptr};

    MythUICheckBox *m_podcastCheck     {nullptr};

  private slots:
    void Save(void);
};

#endif /* MYTHNEWSEDITOR_H */
