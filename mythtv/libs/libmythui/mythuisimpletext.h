#ifndef MYTHUI_SIMPLE_TEXT_H_
#define MYTHUI_SIMPLE_TEXT_H_

// QT headers
#include <QColor>

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
    MythUISimpleText(MythUIType *parent, const QString &name)
        : MythUIType(parent, name) {}
    MythUISimpleText(const QString &text, MythFontProperties font,
                     QRect rect, Qt::Alignment align,
                     MythUIType *parent, const QString &name);
    ~MythUISimpleText() override = default;

  protected:
    void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                  int alphaMod, QRect clipRect) override; // MythUIType

    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType

    Qt::Alignment m_justification {Qt::AlignLeft | Qt::AlignTop};
    MythFontProperties m_font;
    QString m_message;
};

#endif
