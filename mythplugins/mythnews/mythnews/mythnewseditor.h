#ifndef MYTHNEWSEDITOR_H
#define MYTHNEWSEDITOR_H

// MythTV headers
#include <mythtv/httpcomms.h>

// MythUI headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuitextedit.h>
#include <mythtv/libmythui/mythuicheckbox.h>

// MythNews headers
#include "newsengine.h"
#include "newsdbutil.h"

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
