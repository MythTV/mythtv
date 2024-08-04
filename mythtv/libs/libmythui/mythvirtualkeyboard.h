#ifndef MYTHUIVIRTUALKEYBOARD_H_
#define MYTHUIVIRTUALKEYBOARD_H_

#include "mythscreentype.h"

class QKeyEvent;

/// Preferred position to place virtual keyboard popup.
enum PopupPosition : std::uint8_t
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

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
class KeyEventDefinition
{
  public:
    int key() const { return keyCode; };
    Qt::KeyboardModifiers keyboardModifiers() const { return modifiers; };

    int keyCode {0};
    Qt::KeyboardModifiers modifiers;
};
#endif

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
class MUI_PUBLIC MythUIVirtualKeyboard : public MythScreenType
{
  Q_OBJECT

  public:
    MythUIVirtualKeyboard(MythScreenStack *parentStack,  MythUITextEdit *m_parentEdit);
    ~MythUIVirtualKeyboard() override = default;
    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType

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
    static QString decodeChar(QString c);
    QString getKeyText(const KeyDefinition& key) const;
    static void loadEventKeyDefinitions(
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        KeyEventDefinition *keyDef,
#else
        QKeyCombination *keyDef,
#endif
        const QString &action);

    MythUITextEdit *m_parentEdit   {nullptr};
    PopupPosition   m_preferredPos;

    QMap<QString, KeyDefinition> m_keyMap;

    MythUIButton *m_lockButton     {nullptr};
    MythUIButton *m_altButton      {nullptr};
    MythUIButton *m_compButton     {nullptr};
    MythUIButton *m_shiftLButton   {nullptr};
    MythUIButton *m_shiftRButton   {nullptr};

    bool          m_shift          {false};
    bool          m_alt            {false};
    bool          m_lock           {false};

    bool          m_composing      {false};
    QString       m_composeStr;

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    KeyEventDefinition m_upKey;
    KeyEventDefinition m_downKey;
    KeyEventDefinition m_leftKey;
    KeyEventDefinition m_rightKey;
    KeyEventDefinition m_newlineKey;
#else
    QKeyCombination m_upKey;
    QKeyCombination m_downKey;
    QKeyCombination m_leftKey;
    QKeyCombination m_rightKey;
    QKeyCombination m_newlineKey;
#endif
};

#endif
