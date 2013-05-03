#ifndef MYTHTHEMEDMENU_H_
#define MYTHTHEMEDMENU_H_

#include "mythscreentype.h"
#include "mythdialogbox.h"
#include "mythuistatetype.h"
#include "mythuibuttonlist.h"
#include "xmlparsebase.h"

class MythMainWindow;
class MythThemedMenuState;

class QKeyEvent;

struct ThemedButton
{
    QString type;
    QStringList action;
    QString text;
    QString description;
    MythImage *icon;
    bool active;
    QString password;
};

struct ButtonIcon
{
    QString name;
    MythImage *icon;
    MythImage *activeicon;
    QPoint offset;
};


/** \class MyththemedMenuState
 *  \brief Private class that controls the settings of buttons, logos,
 *         backgrounds, texts, and more, for the MythThemedMenu class.
 */
class MUI_PUBLIC MythThemedMenuState : public MythScreenType
{
  public:
    MythThemedMenuState(MythScreenStack *parent, const QString &name);
   ~MythThemedMenuState();

    bool Create(void);

    void (*m_callback)(void *, QString &);
    void *m_callbackdata;

    bool m_killable;

    bool m_loaded;
    MythUIStateType *m_titleState;
    MythUIStateType *m_watermarkState;
    MythUIButtonList *m_buttonList;
    MythUIText *m_descriptionText;

  protected:
    void CopyFrom(MythUIType*);
};

/// \brief Themed menu class, used for main menus in %MythTV frontend
class MUI_PUBLIC MythThemedMenu : public MythThemedMenuState
{
    Q_OBJECT
  public:
    MythThemedMenu(const QString &cdir, const QString &menufile,
                    MythScreenStack *parent, const QString &name,
                    bool allowreorder = false, MythThemedMenuState *state = NULL);
   ~MythThemedMenu();

    bool foundTheme(void);

    void setCallback(void (*lcallback)(void *, QString &), void *data);
    void setKillable(void);

    QString getSelection(void);

    virtual void aboutToShow(void);

    void ShowMenu();
    void aboutScreen();
    void customEvent(QEvent *event);
    void mediaEvent(MythMediaEvent *event);
    
  protected:
    virtual bool keyPressEvent(QKeyEvent *e);

  private slots:
    void setButtonActive(MythUIButtonListItem* item);
    void buttonAction(MythUIButtonListItem* item, bool skipPass = false);

  private:
    void SetMenuTheme(const QString &menufile);

    bool parseMenu(const QString &menuname);
    void parseThemeButton(QDomElement &element);

    void addButton(const QString &type, const QString &text,
                   const QString &alttext, const QStringList &action,
                   const QString &description, const QString &password);

    bool handleAction(const QString &action, const QString &password = QString());
    bool findDepends(const QString &fileList);
    QString findMenuFile(const QString &menuname);

    bool checkPinCode(const QString &password_setting);

    MythThemedMenu *m_parent;

    MythThemedMenuState *m_state;
    bool m_allocedstate;

    QString m_selection;
    bool m_foundtheme;

    bool m_ignorekeys;

    bool m_wantpop;

    QString m_menumode;

    MythDialogBox* m_menuPopup;
};

Q_DECLARE_METATYPE(ThemedButton)

#endif
