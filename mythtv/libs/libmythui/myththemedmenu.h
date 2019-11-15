#ifndef MYTHTHEMEDMENU_H_
#define MYTHTHEMEDMENU_H_

#include "mythscreentype.h"
#include "mythdialogbox.h"
#include "mythuistatetype.h"
#include "mythuibuttonlist.h"

class MythMainWindow;
class MythThemedMenuState;

class QKeyEvent;

struct ThemedButton
{
    QString type;
    QStringList action;
    QString text;
    QString alttext;
    QString description;
    MythImage *icon;
    bool active;
    QString password;
};


/** \class MyththemedMenuState
 *  \brief Private class that controls the settings of buttons, logos,
 *         backgrounds, texts, and more, for the MythThemedMenu class.
 */
class MUI_PUBLIC MythThemedMenuState : public MythScreenType
{
  public:
    MythThemedMenuState(MythScreenStack *parent, const QString &name)
        : MythScreenType(parent, name) {}
   ~MythThemedMenuState() = default;

    bool Create(void) override; // MythScreenType

    void (*m_callback)(void *, QString &) {nullptr};
    void *m_callbackdata {nullptr};

    bool              m_killable        {false};

    bool              m_loaded          {false};
    MythUIStateType  *m_titleState      {nullptr};
    MythUIStateType  *m_watermarkState  {nullptr};
    MythUIButtonList *m_buttonList      {nullptr};
    MythUIText       *m_descriptionText {nullptr};

  protected:
    void CopyFrom(MythUIType*) override; // MythScreenType
};

/// \brief Themed menu class, used for main menus in %MythTV frontend
class MUI_PUBLIC MythThemedMenu : public MythThemedMenuState
{
    Q_OBJECT
  public:
    MythThemedMenu(const QString &cdir, const QString &menufile,
                    MythScreenStack *parent, const QString &name,
                    bool allowreorder = false, MythThemedMenuState *state = nullptr);
   ~MythThemedMenu();

    bool foundTheme(void);

    void getCallback(void (**lcallback)(void *, QString &), void **data);
    void setCallback(void (*lcallback)(void *, QString &), void *data);
    void setKillable(void);

    QString getSelection(void);

    void aboutToShow(void) override; // MythScreenType

    void ShowMenu() override; // MythScreenType
    void aboutScreen();
    void customEvent(QEvent *event) override; // MythUIType
    void mediaEvent(MythMediaEvent *event) override; // MythUIType
    
  protected:
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

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
    static bool findDepends(const QString &fileList);
    static bool findDependsExec(const QString &filename);
    static QString findMenuFile(const QString &menuname);

    bool checkPinCode(const QString &password_setting);

    MythThemedMenu *m_parent     {nullptr};

    MythThemedMenuState *m_state {nullptr};
    bool m_allocedstate          {false};

    QString m_selection;
    bool m_foundtheme            {false};
    bool m_ignorekeys            {false};
    bool m_wantpop               {false};

    QString m_menumode;

    MythDialogBox* m_menuPopup   {nullptr};
};

Q_DECLARE_METATYPE(ThemedButton)

#endif
