#ifndef MYTHNEWSEDITOR_H
#define MYTHNEWSEDITOR_H

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
                   const char *name);
    ~MythNewsEditor();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

private:
    NewsSite *m_site;
    QString m_siteName;
    bool m_editing;

    MythUIText *m_titleText;
    MythUIText *m_nameLabelText;
    MythUIText *m_urlLabelText;
    MythUIText *m_iconLabelText;

    MythUITextEdit *m_nameEdit;
    MythUITextEdit *m_urlEdit;
    MythUITextEdit *m_iconEdit;

    MythUIButton *m_okButton;
    MythUIButton *m_cancelButton;

    MythUICheckBox *m_podcastCheck;

private slots:
    void Save(void);
};

#endif /* MYTHNEWSEDITOR_H */
