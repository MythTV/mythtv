#ifndef MYTHUI_TEXTEDIT_H_
#define MYTHUI_TEXTEDIT_H_

#include <QString>

#include "mythuitype.h"
#include "mythuitext.h"
#include "mythuistatetype.h"
#include "mythuiimage.h"

class MythFontProperties;

enum InputFilter
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
class MythUITextEdit : public MythUIType, public StorageUser
{
    Q_OBJECT

  public:
    MythUITextEdit(MythUIType *parent, const QString &name);
   ~MythUITextEdit();

    virtual void Pulse(void);
    virtual bool keyPressEvent(QKeyEvent *);

    void SetText(const QString text, bool moveCursor = true);
    QString GetText(void) const { return m_Message; }

    void SetFilter(InputFilter filter) { m_Filter = filter; }
    void SetPassword(bool isPassword)  { m_isPassword = isPassword; }

    enum MoveDirection { MoveLeft, MoveRight, MoveEnd };
    bool MoveCursor(MoveDirection);

    // StorageUser
    void SetDBValue(const QString &text) { SetText(text); }
    QString GetDBValue(void) const { return GetText(); }

  signals:
    void valueChanged();

  public slots:
    void Select();
    void Deselect();

  protected:
    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    void Init(void);
    void SetInitialStates(void);

    bool InsertCharacter(const QString character);
    void RemoveCharacter(void);

    void SetMaxLength(const int length);

    bool m_initialized;

    int m_blinkInterval;
    int m_cursorBlinkRate;

    int m_maxLength;

    QString m_Message;
    InputFilter m_Filter;
    int m_Position;

    bool m_isPassword;

    MythUIStateType *m_backgroundState;
    MythUIImage *m_cursorImage;
    MythUIText  *m_Text;
};

#endif
