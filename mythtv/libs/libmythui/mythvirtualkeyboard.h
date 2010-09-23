#ifndef MYTHUIVIRTUALKEYBOARD_H_
#define MYTHUIVIRTUALKEYBOARD_H_

#include "mythscreentype.h"

class QKeyEvent;

/// Preferred position to place virtual keyboard popup.
enum PopupPosition
{
    VK_POSABOVEEDIT = 1,
    VK_POSBELOWEDIT,
    VK_POSTOPDIALOG,
    VK_POSBOTTOMDIALOG,
    VK_POSCENTERDIALOG
};

struct KeyDefinition
{
    QString name;
    QString type;
    QString normal, shift, alt, altshift;
    QString up, down, left, right;
};

/** \class MythUIVirtualKeyboard
 *
 * \brief A popup onscreen keyboard for easy alphanumeric and text entry using
 *        a remote control or mouse.
 *
 * The virtual keyboard is invoked by MythUITextEdit when the user triggers the
 * SELECT action. It is not normally necessary to use this widget explicitly.
 *
 * \ingroup MythUI_Widgets
 */
class MPUBLIC MythUIVirtualKeyboard : public MythScreenType
{
  Q_OBJECT

  public:
    MythUIVirtualKeyboard(MythScreenStack *parentStack,  MythUITextEdit *m_parentEdit);
    ~MythUIVirtualKeyboard();
    bool Create(void);
    bool keyPressEvent(QKeyEvent *e);

  signals:
    void keyPressed(QString key);

  protected slots:
    void charClicked(void);
    void shiftClicked(void);
    void delClicked(void);
    void lockClicked(void);
    void altClicked(void);
    void compClicked(void);
    void moveleftClicked(void);
    void moverightClicked(void);
    void backClicked(void);
    void returnClicked(void);

  private:
    void loadKeyDefinitions(const QString &lang);
    void parseKey(const QDomElement &element);
    void updateKeys(bool connectSignals = false);
    QString decodeChar(QString c);
    QString getKeyText(KeyDefinition key);

    MythUITextEdit *m_parentEdit;
    PopupPosition   m_preferredPos;

    QMap<QString, KeyDefinition> m_keyMap;

    MythUIButton *m_lockButton;
    MythUIButton *m_altButton;
    MythUIButton *m_compButton;
    MythUIButton *m_shiftLButton;
    MythUIButton *m_shiftRButton;

    bool          m_shift;
    bool          m_alt;
    bool          m_lock;

    bool          m_composing;
    QString       m_composeStr;
};

#endif
