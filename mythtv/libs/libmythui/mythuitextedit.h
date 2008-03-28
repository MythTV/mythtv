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
    MythUITextEdit(MythUIType *parent, const char *name);
   ~MythUITextEdit();

    virtual void Pulse(void);
    virtual bool keyPressEvent(QKeyEvent *);

    void SetBackgroundImage(MythImage *);
    void SetCursorImage(MythImage *);
    void SetMaxLength(const int length);
    void SetPaddingMargin(const int margin);
    void SetText(const QString text);

    enum MoveDirection { MoveLeft, MoveRight, MoveEnd };
    void MoveCursor(MoveDirection);

  signals:
    void Finished();

  protected:
    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    void InsertCharacter(const QString character);
    void RemoveCharacter(void);

    int m_blinkInterval;
    int m_cursorBlinkRate;
    int m_maxLength;

    bool m_cursorVisible;
    bool m_showCursor;

    int m_Justification;
    int m_PaddingMargin;

    QString m_Message;
    int m_position;

    MythUIImage *m_backgroundImage;
    MythUIImage *m_cursorImage;
    MythUIText  *m_Text;
};

#endif
