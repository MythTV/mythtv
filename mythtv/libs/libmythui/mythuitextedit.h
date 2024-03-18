#ifndef MYTHUI_TEXTEDIT_H_
#define MYTHUI_TEXTEDIT_H_

#include <QString>
#include <QClipboard>

#include "libmythbase/mythstorage.h"
#include "libmythbase/mythtimer.h"
#include "libmythui/mythuitype.h"
#include "libmythui/mythvirtualkeyboard.h"

class QInputMethodEvent;

class MythFontProperties;
class MythUIStateType;
class MythUIImage;
class MythUIText;

enum InputFilter : std::uint8_t
{
    FilterNone = 0x0,
    FilterAlpha = 0x01,
    FilterNumeric = 0x02,
    FilterAlphaNumeric = 0x03,
    FilterSymbols = 0x04,
    FilterPunct = 0x08
};

/** \class MythUITextEdit
 *
 *  \brief A text entry and edit widget
 *
 */
class MUI_PUBLIC MythUITextEdit : public MythUIType, public StorageUser
{
    Q_OBJECT

  public:
    MythUITextEdit(MythUIType *parent, const QString &name);
   ~MythUITextEdit() override = default;

    void Pulse(void) override; // MythUIType
    bool keyPressEvent(QKeyEvent *event) override; // MythUIType
    bool inputMethodEvent(QInputMethodEvent *event) override; // MythUIType
    bool gestureEvent(MythGestureEvent *event) override; // MythUIType
    void Reset(void) override; // MythUIType

    void SetText(const QString &text, bool moveCursor = true);
    void InsertText(const QString &text);
    QString GetText(void) const { return m_message; }

    void SetFilter(InputFilter filter) { m_filter = filter; }
    void SetPassword(bool isPassword)  { m_isPassword = isPassword; }
    void SetMaxLength(int length);

    enum MoveDirection : std::uint8_t
                       { MoveLeft, MoveRight, MoveUp, MoveDown, MovePageUp, MovePageDown, MoveEnd };
    bool MoveCursor(MoveDirection moveDir);

    void SetKeyboardPosition(PopupPosition pos) { m_keyboardPosition = pos; }
    PopupPosition GetKeyboardPosition(void)  { return m_keyboardPosition; }

    // StorageUser
    void SetDBValue(const QString &text) override { SetText(text); } // StorageUser
    QString GetDBValue(void) const override { return GetText(); } // StorageUser

  signals:
    void valueChanged();

  public slots:
    void Select();
    void Deselect();

  protected:
    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    void Finalize(void) override; // MythUIType

    void Init(void);
    void SetInitialStates(void);

    bool InsertCharacter(const QString &character);
    void RemoveCharacter(int position);

    void CutTextToClipboard(void);
    void CopyTextToClipboard(void);
    void PasteTextFromClipboard(QClipboard::Mode mode = QClipboard::Clipboard);
    bool UpdateTmpString(const QString &str);

    bool m_initialized                 { false };

    int m_blinkInterval                { 0 };
    int m_cursorBlinkRate              { 40 };
    MythTimer m_lastKeyPress;

    int m_maxLength                    { 255 };

    QString m_message;
    InputFilter m_filter               { FilterNone };
    int m_position                     { -1 };

    bool m_isPassword                  { false };

    PopupPosition m_keyboardPosition   { VK_POSBELOWEDIT };

    MythUIStateType *m_backgroundState { nullptr };
    MythUIImage *m_cursorImage         { nullptr };
    MythUIText  *m_text                { nullptr };

    int m_composeKey                   { 0 };

    bool m_isIMEinput                  { false };
    QString m_messageBak;
};

#endif
