#ifndef MYTHUI_TEXTEDIT_H_
#define MYTHUI_TEXTEDIT_H_

#include <qstring.h>

#include "mythuitype.h"
#include "mythuitext.h"
#include "mythuiimage.h"

class MythFontProperties;

/** \class MythUITextEdit
 *
 * \brief A text entry and edit widget
 *
 */
class MythUITextEdit : public MythUIType
{
    Q_OBJECT

  public:
    MythUITextEdit(MythUIType *parent, const char *name, bool doInit = true);
   ~MythUITextEdit();

    virtual void Pulse(void);
    virtual bool keyPressEvent(QKeyEvent *);

    void SetBackgroundImage(MythImage *);
    void SetCursorImage(MythImage *);
    void SetMaxLength(const int length);
    void SetPaddingMargin(const int margin);
    void SetTextRect(const QRect area=QRect(0,0,0,0));
    void SetText(const QString text, bool moveCursor = true);

    enum InputFilter
    {
        FilterNone = 0x0,
        FilterAlpha = 0x01,
        FilterNumeric = 0x02,
        FilterAlphaNumeric = 0x03,
        FilterSymbols = 0x04,
        FilterPunct = 0x08
    };
    void SetFilter(InputFilter filter) { m_Filter = filter; }

    enum MoveDirection { MoveLeft, MoveRight, MoveEnd };
    void MoveCursor(MoveDirection);

  signals:
    void Finished();

  protected:
    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    void Init(void);

    bool InsertCharacter(const QString character);
    void RemoveCharacter(void);

    int m_blinkInterval;
    int m_cursorBlinkRate;
    bool m_showCursor;

    int m_maxLength;

    int m_Justification;
    int m_PaddingMargin;

    QString m_Message;
    InputFilter m_Filter;
    int m_Position;

    MythUIImage *m_backgroundImage;
    MythUIImage *m_cursorImage;
    MythUIText  *m_Text;
};

#endif
