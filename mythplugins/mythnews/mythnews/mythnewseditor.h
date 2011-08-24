#ifndef MYTHNEWSEDITOR_H
#define MYTHNEWSEDITOR_H

// Qt headers
#include <QMutex>
#include <QString>

// MythTV headers
#include <mythscreentype.h>

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
   ~MythNewsEditor();

    bool Create(void);
    bool keyPressEvent(QKeyEvent*);

  private:
    mutable QMutex  m_lock;
    NewsSite       *m_site;
    QString         m_siteName;
    bool            m_editing;

    MythUIText     *m_titleText;
    MythUIText     *m_nameLabelText;
    MythUIText     *m_urlLabelText;
    MythUIText     *m_iconLabelText;
    MythUIText     *m_podcastLabelText;

    MythUITextEdit *m_nameEdit;
    MythUITextEdit *m_urlEdit;
    MythUITextEdit *m_iconEdit;

    MythUIButton   *m_okButton;
    MythUIButton   *m_cancelButton;

    MythUICheckBox *m_podcastCheck;

  private slots:
    void Save(void);
};

#endif /* MYTHNEWSEDITOR_H */
