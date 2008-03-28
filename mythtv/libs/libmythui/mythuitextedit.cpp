#include <qapplication.h>
#include <qregexp.h>
#include <QChar>

#include "mythuitextedit.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

#include "mythcontext.h"

MythUITextEdit::MythUITextEdit(MythUIType *parent, const char *name)
           : MythUIType(parent, name)
{
    m_Message = "";

    m_Area = QRect();

    m_Justification = (Qt::AlignLeft | Qt::AlignTop);

    m_showCursor = false;
    m_cursorVisible = false;
    m_blinkInterval = 0;
    m_cursorBlinkRate = 40;

    m_position = -1;

    m_PaddingMargin = 0;

    m_backgroundImage = new MythUIImage(this, "backgroundimage");
    m_Text = new MythUIText(this, "TextEdit");
    m_cursorImage = new MythUIImage(this, "cursorimage");
    m_Text->SetCutDown(false);

    m_CanHaveFocus = true;

    m_cursorImage->SetVisible(false);
}

MythUITextEdit::~MythUITextEdit()
{
    if (m_backgroundImage)
        delete m_backgroundImage;
    if (m_cursorImage)
        delete m_cursorImage;
    if (m_Text)
        delete m_Text;
}

void MythUITextEdit::Pulse(void)
{
    if (m_showCursor)
    {
        if (m_blinkInterval > m_cursorBlinkRate)
        {
            m_blinkInterval = 0;
            if (m_cursorVisible)
            {
                m_cursorImage->SetVisible(false);
                m_cursorVisible = false;
            }
            else
            {
                m_cursorImage->SetVisible(true);
                m_cursorVisible = true;
            }
        }

        m_blinkInterval++;
    }

    MythUIType::Pulse();
}

bool MythUITextEdit::ParseElement(QDomElement &element)
{
    bool parsed = true;

    if (element.tagName() == "area")
    {
        m_Area = parseRect(element);
        m_Text->SetArea(m_Area);
    }
    else if (element.tagName() == "font")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *fp = GetFont(fontname);
        if (!fp)
            fp = GetGlobalFontMap()->GetFont(fontname);
        if (fp)
            m_Text->SetFontProperties(*fp);
    }
    else if (element.tagName() == "value")
    {
        if (element.attribute("lang","").isEmpty())
        {
            m_Message = qApp->translate("ThemeUI", getFirstText(element));
        }
        else if (element.attribute("lang","").lower() ==
                 gContext->GetLanguageAndVariant())
        {
            m_Message = getFirstText(element);
        }
        else if (element.attribute("lang","").lower() ==
                 gContext->GetLanguage())
        {
            m_Message = getFirstText(element);
        }
    }
    else if (element.tagName() == "multiline")
    {
        if (parseBool(element))
            m_Justification |= Qt::WordBreak;
        else
            m_Justification &= ~Qt::WordBreak;

        m_Text->SetJustification(m_Justification);
    }
    else if (element.tagName() == "align")
    {
        QString align = getFirstText(element).lower();

        // preserve the wordbreak attribute, drop everything else
        m_Justification = m_Justification & Qt::WordBreak;

        m_Justification |= parseAlignment(align);

        m_Text->SetJustification(m_Justification);
    }
    else if (element.tagName() == "maxlength")
    {
        QString maxlength = getFirstText(element);
        SetMaxLength(maxlength.toInt());
    }
    else if (element.tagName() == "margin")
    {
        int paddingMargin = NormX(getFirstText(element).toInt());
        SetPaddingMargin(paddingMargin);
    }
    else if (element.tagName() == "cursor")
    {
        MythImage *tmp;
        QString filename = element.attribute("filename");

        if (!filename.isEmpty())
        {
            tmp = GetMythPainter()->GetFormatImage();
            tmp->Load(filename);
            SetCursorImage(tmp);
        }
    }
    else if (element.tagName() == "background")
    {
        MythImage *tmp;
        QString filename = element.attribute("filename");

        tmp = GetMythPainter()->GetFormatImage();

        if (filename.isEmpty() || !tmp->Load(filename))
        {
            QColor startcol = QColor("#EEEEEE");
            QColor endcol = QColor("#AEAEAE");
            int alpha = 255;
            tmp = MythImage::Gradient(QSize(10,10), startcol,
                            endcol, alpha);
        }

        SetBackgroundImage(tmp);
    }
    else
        parsed = false;

    return parsed;
}

void MythUITextEdit::SetCursorImage(MythImage *image)
{
    if (image && !image->isNull())
    {
        QFontMetrics fm(m_Text->GetFontProperties()->face());
        int height = fm.height();
        if (height > 0)
        {
            int width = int(((float)height / (float)image->height())
                            * (float)image->width());
            image->Resize(QSize(width,height));
        }
        m_cursorImage->SetImage(image);

        m_cursorImage->SetVisible(false);
        m_showCursor = true;
    }
}

void MythUITextEdit::SetBackgroundImage(MythImage *image)
{
    if (image)
    {
        image->Resize(QSize(m_Area.width(), m_Area.height()));
        m_backgroundImage->SetPosition(0,0);
        m_backgroundImage->SetImage(image);
    }
}

void MythUITextEdit::SetMaxLength(const int length)
{
    m_maxLength = length;
}

void MythUITextEdit::SetPaddingMargin(const int margin)
{
    m_PaddingMargin = margin;

    QRect textrect;
    textrect = QRect(   0 + margin,
                        0 + margin,
                        m_Area.width() - margin * 2,
                        m_Area.height() - margin * 2);
    m_Text->SetArea(textrect);
    m_cursorImage->SetPosition(margin,margin);
}

void MythUITextEdit::SetText(QString text)
{
    m_Text->SetText(text);
    MoveCursor(MoveEnd);
}

void MythUITextEdit::InsertCharacter(const QString character)
{
    if (m_maxLength != 0 && m_Message.length() == m_maxLength)
        return;

    m_Message.append(character);
    SetText(m_Message);
}

void MythUITextEdit::RemoveCharacter()
{
    if (m_Message.isEmpty())
        return;

    m_Message.remove(m_Message.length()-1, 1);
    SetText(m_Message);
}

void MythUITextEdit::MoveCursor(MoveDirection moveDir)
{
    if (!m_showCursor)
        return;

    QFontMetrics fm(m_Text->GetFontProperties()->face());

    int cursorPos = m_cursorImage->GetArea().x();
    QRect textRect = m_Text->GetArea();
    int newcursorPos = 0;
    QSize size;

    switch (moveDir)
    {
        case MoveLeft:
            if (m_position < 0)
                return;

            size = fm.size(Qt::SingleLine, m_Message.mid(m_position,1));

            newcursorPos = cursorPos - size.width();

            if (newcursorPos < 0)
            {
                newcursorPos = m_PaddingMargin;
                if (m_position == 0)
                    m_Text->SetStartPosition(0, 0);
                else
                    m_Text->MoveStartPosition(size.width(), 0);
            }

            m_position--;
            break;
        case MoveRight:
            if (m_position == (m_Message.size() - 1))
                return;

            size = fm.size(Qt::SingleLine, m_Message.mid(m_position+1,1));

            newcursorPos = cursorPos + size.width();

            if (newcursorPos > textRect.width())
            {
                newcursorPos = cursorPos;
                m_Text->MoveStartPosition(-(size.width()), 0);
            }

            m_position++;
            break;
        case MoveEnd:
            size = fm.size(Qt::SingleLine, m_Message);

            newcursorPos = size.width() + m_cursorImage->GetArea().width();

            if (newcursorPos <= 0)
                newcursorPos = m_PaddingMargin;

            if (newcursorPos >= textRect.width())
            {
                QRect drawRect = m_Text->GetDrawRect();
                m_Text->MoveStartPosition(drawRect.width()-newcursorPos, 0);
                newcursorPos = textRect.width()-m_cursorImage->GetArea().width();
            }
            else
            {
                m_Text->SetStartPosition(0,0);
            }

            m_position = m_Message.size() - 1;

            break;
    }

    m_cursorImage->SetPosition(newcursorPos, textRect.y());

    SetRedraw();
}

bool MythUITextEdit::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    bool handled = false;

    GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {

        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
        {
            MoveCursor(MoveLeft);
        }
        else if (action == "RIGHT")
        {
            MoveCursor(MoveRight);
        }
        else if (action == "BACKSPACE" || action == "DELETE")
        {
            RemoveCharacter();
        }
        else
            handled = false;
    }

    const QChar *character = e->text().unicode();
    if (!handled && (character->isLetterOrNumber() || character->isPunct()
        || character->isSpace() || character->isSymbol()))
    {
         InsertCharacter(e->text());
        handled = true;
    }

    return handled;
}

void MythUITextEdit::CopyFrom(MythUIType *base)
{
    MythUITextEdit *textedit = dynamic_cast<MythUITextEdit *>(base);
    if (!textedit)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_blinkInterval = textedit->m_blinkInterval;
    m_cursorBlinkRate = textedit->m_cursorBlinkRate;
    m_cursorVisible = textedit->m_cursorVisible;
    m_showCursor = textedit->m_showCursor;

    m_maxLength = textedit->m_maxLength;

    m_Justification = textedit->m_Justification;

    m_Message = textedit->m_Message;

    m_position = textedit->m_position;

    m_backgroundImage = dynamic_cast<MythUIImage *>
                    (GetChild("backgroundimage"));
    m_cursorImage = dynamic_cast<MythUIImage *>
                    (GetChild("cursorimage"));

    MythUIType::CopyFrom(base);
}

void MythUITextEdit::CreateCopy(MythUIType *parent)
{
    MythUITextEdit *textedit = new MythUITextEdit(parent, name());
    textedit->CopyFrom(this);
}
