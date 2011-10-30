#ifndef MYTHUI_SIMPLE_TEXT_H_
#define MYTHUI_SIMPLE_TEXT_H_

// QT headers
#include <QColor>

// Mythdb headers
#include "mythstorage.h"

// Mythui headers
#include "mythuitype.h"
#include "mythfontproperties.h"

class MythFontProperties;

/**
 *  \class MythUISimpleText
 *
 *  \brief Simplified text widget, displays a text string
 *
 *  Font and alignment may be applied to the text in this widget.
 *
 *  \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUISimpleText : public MythUIType
{
  public:
    MythUISimpleText(MythUIType *parent, const QString &name);
    MythUISimpleText(const QString &text, const MythFontProperties &font,
		     const QRect &rect, Qt::Alignment align,
		     MythUIType *parent, const QString &name);
    ~MythUISimpleText();

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

#if 0 // Any reason to support this in a theme?
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void Finalize(void);
#endif
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    int m_Justification;
    QString m_Message;
    MythFontProperties m_Font;

#if 0 // Any reason to support this in a theme?
    friend class MythUIButton;
    friend class MythThemedMenu;
    friend class MythThemedMenuPrivate;
#endif
};

#endif
