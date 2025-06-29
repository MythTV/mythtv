
#include "mythuitext.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QtGlobal>
#include <QDomDocument>
#include <QFontMetrics>
#include <QString>
#include <QHash>
#include <QRegularExpression>

#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "mythuihelper.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

MythUIText::MythUIText(MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_font(new MythFontProperties())
{
    m_enableInitiator = true;

    m_fontStates.insert("default", MythFontProperties());
    *m_font = m_fontStates["default"];
}

MythUIText::MythUIText(const QString &text, const MythFontProperties &font,
                       QRect displayRect, QRect altDisplayRect,
                       MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_origDisplayRect(displayRect), m_altDisplayRect(altDisplayRect),
      m_canvas(0, 0, displayRect.width(), displayRect.height()),
      m_drawRect(displayRect),
      m_message(text.trimmed()),
      m_defaultMessage(text),
      m_font(new MythFontProperties())
{
#if 0 // Not currently used
    m_usingAltArea = false;
#endif

    m_enableInitiator = true;

    MythUIText::SetArea(MythRect(displayRect));
    m_fontStates.insert("default", font);
    *m_font = m_fontStates["default"];
}

MythUIText::~MythUIText()
{
    delete m_font;
    m_font = nullptr;

    for (auto *layout : std::as_const(m_layouts))
        delete layout;
}

void MythUIText::Reset()
{
    if (m_message != m_defaultMessage)
    {
        SetText(m_defaultMessage);
        SetRedraw();
        emit DependChanged(true);
    }

    SetFontState("default");

    MythUIType::Reset();
}

void MythUIText::ResetMap(const InfoMap &map)
{
    QString newText = GetTemplateText();

    if (newText.isEmpty())
        newText = GetDefaultText();

    static const QRegularExpression re {R"(%(([^\|%]+)?\||\|(.))?([\w#]+)(\|(.+?))?%)",
                           QRegularExpression::DotMatchesEverythingOption};

    bool replaced = map.contains(objectName());

    if (!replaced && !newText.isEmpty() && newText.contains(re))
    {
        QString translatedTemplate = QCoreApplication::translate("ThemeUI",
                                                     newText.toUtf8());

        QRegularExpressionMatchIterator i = re.globalMatch(translatedTemplate);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            QString key = match.captured(4).toLower().trimmed();

            if (map.contains(key))
            {
                replaced = true;
                break;
            }
        }
    }

    if (replaced)
    {
        Reset();
    }
}

void MythUIText::SetText(const QString &text)
{
    const QString& newtext = text;

    if (!m_layouts.isEmpty() && newtext == m_message)
        return;

    if (newtext.isEmpty())
    {
        m_message = m_defaultMessage;
        emit DependChanged(true);
    }
    else
    {
        m_message = newtext;
        emit DependChanged(false);
    }
    m_cutMessage.clear();
    FillCutMessage();

    SetRedraw();
}

void MythUIText::SetTextFromMap(const InfoMap &map)
{
    QString newText = GetTemplateText();

    if (newText.isEmpty())
        newText = GetDefaultText();

    static const QRegularExpression re {R"(%(([^\|%]+)?\||\|(.))?([\w#]+)(\|(.+?))?%)",
                           QRegularExpression::DotMatchesEverythingOption};

    if (!newText.isEmpty() && newText.contains(re))
    {
        QString translatedTemplate = QCoreApplication::translate("ThemeUI",
                                                     newText.toUtf8());

        QString tempString = translatedTemplate;
        bool replaced = map.contains(objectName());

        QRegularExpressionMatchIterator i = re.globalMatch(translatedTemplate);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            QString key = match.captured(4).toLower().trimmed();
            QString replacement;

            if (map.contains(key))
            {
                replaced = true;
            }
            if (!map.value(key).isEmpty())
            {
                replacement = QString("%1%2%3%4")
                .arg(match.captured(2),
                     match.captured(3),
                     map.value(key),
                     match.captured(6));
            }

            tempString.replace(match.captured(0), replacement);
        }
        if (replaced)
        {
            SetText(tempString);
        }
    }
    else if (map.contains(objectName()))
    {
        SetText(map.value(objectName()));
    }
}

void MythUIText::SetFontProperties(const MythFontProperties &fontProps)
{
    m_fontStates.insert("default", fontProps);
    if (m_font->GetHash() != m_fontStates["default"].GetHash())
    {
        *m_font = m_fontStates["default"];
        if (!m_message.isEmpty())
        {
            FillCutMessage();
            SetRedraw();
        }
    }
}

void MythUIText::SetFontState(const QString &state)
{
    if (m_fontStates.contains(state))
    {
        if (m_font->GetHash() == m_fontStates[state].GetHash())
            return;
        *m_font = m_fontStates[state];
    }
    else
    {
        if (m_font->GetHash() == m_fontStates["default"].GetHash())
            return;
        *m_font = m_fontStates["default"];
    }
    if (!m_message.isEmpty())
    {
        FillCutMessage();
        SetRedraw();
    }
}

#if 0 // Not currently used
void MythUIText::UseAlternateArea(bool useAlt)
{
    if (useAlt && m_altDisplayRect.width() > 1)
    {
        MythUIType::SetArea(m_altDisplayRect);
        m_usingAltArea = true;
    }
    else
    {
        MythUIType::SetArea(m_origDisplayRect);
        m_usingAltArea = false;
    }

    FillCutMessage();
}
#endif

void MythUIText::SetJustification(int just)
{
    int h = just & Qt::AlignHorizontal_Mask;
    int v = just & Qt::AlignVertical_Mask;

    if ((h && (m_justification & Qt::AlignHorizontal_Mask) ^ h) ||
        (v && (m_justification & Qt::AlignVertical_Mask) ^ v))
    {
        // preserve the wordbreak attribute, drop everything else
        m_justification = m_justification & Qt::TextWordWrap;
        m_justification |= just;
        if (!m_message.isEmpty())
        {
            FillCutMessage();
            SetRedraw();
        }
    }
}

int MythUIText::GetJustification(void) const
{
    return m_justification;
}

void MythUIText::SetCutDown(Qt::TextElideMode mode)
{
    if (mode != m_cutdown)
    {
        m_cutdown = mode;
        if (m_scrolling && m_cutdown != Qt::ElideNone)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("'%1' (%2): <scroll> and "
                                             "<cutdown> are not combinable.")
                .arg(objectName(), GetXMLLocation()));
            m_cutdown = Qt::ElideNone;
        }
        if (!m_message.isEmpty())
        {
            FillCutMessage();
            SetRedraw();
        }
    }
}

void MythUIText::SetMultiLine(bool multiline)
{
    if (multiline != m_multiLine)
    {
        m_multiLine = multiline;

        if (m_multiLine)
            m_justification |= Qt::TextWordWrap;
        else
            m_justification &= ~Qt::TextWordWrap;

        if (!m_message.isEmpty())
        {
            FillCutMessage();
            SetRedraw();
        }
    }
}

void MythUIText::SetArea(const MythRect &rect)
{
    MythUIType::SetArea(rect);
    m_cutMessage.clear();

    m_drawRect = m_area;
    FillCutMessage();
}

void MythUIText::SetPosition(const MythPoint &pos)
{
    MythUIType::SetPosition(pos);
    m_drawRect.moveTopLeft(m_area.topLeft());
}

void MythUIText::SetCanvasPosition(int x, int y)
{
    QPoint newpoint(x, y);

    if (newpoint == m_canvas.topLeft())
        return;

    m_canvas.moveTopLeft(newpoint);
    SetRedraw();
}

void MythUIText::ShiftCanvas(int x, int y)
{
    if (x == 0 && y == 0)
        return;

    m_canvas.moveTop(m_canvas.y() + y);
    m_canvas.moveLeft(m_canvas.x() + x);
    SetRedraw();
}

void MythUIText::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect)
{
    if (m_canvas.isEmpty())
        return;

    FormatVector formats;
    QRect drawrect = m_drawRect.toQRect();
    drawrect.translate(xoffset, yoffset);
    QRect canvas = m_canvas.toQRect();

    int alpha = CalcAlpha(alphaMod);

    if (m_ascent)
    {
        drawrect.setY(drawrect.y() - m_ascent);
        canvas.moveTop(canvas.y() + m_ascent);
        canvas.setHeight(canvas.height() + m_ascent);
    }
    if (m_descent)
    {
        drawrect.setHeight(drawrect.height() + m_descent);
        canvas.setHeight(canvas.height() + m_descent);
    }

    if (m_leftBearing)
    {
        drawrect.setX(drawrect.x() + m_leftBearing);
        canvas.moveLeft(canvas.x() - m_leftBearing);
        canvas.setWidth(canvas.width() - m_leftBearing);
    }
    if (m_rightBearing)
    {
        drawrect.setWidth(drawrect.width() - m_rightBearing);
        canvas.setWidth(canvas.width() - m_rightBearing);
    }

    if (GetFontProperties()->hasOutline())
    {
        QColor outlineColor;
        int    outlineSize = 0;
        int    outlineAlpha = 255;

        GetFontProperties()->GetOutline(outlineColor, outlineSize,
                                        outlineAlpha);

        MythPoint  outline(outlineSize, outlineSize);

        drawrect.setX(drawrect.x() - outline.x());
        drawrect.setWidth(drawrect.width() + outline.x());
        drawrect.setY(drawrect.y() - outline.y());
        drawrect.setHeight(drawrect.height() + outline.y());

        /* Canvas pos is where the view port (drawrect) pulls from, so
         * it needs moved to the right for the left edge to be picked up*/
        canvas.moveLeft(canvas.x() + outline.x());
        canvas.setWidth(canvas.width() + outline.x());
        canvas.moveTop(canvas.y() + outline.y());
        canvas.setHeight(canvas.height() + outline.y());
    }

    if (GetFontProperties()->hasShadow())
    {
        QPoint shadowOffset;
        QColor shadowColor;
        int    shadowAlpha = 255;

        GetFontProperties()->GetShadow(shadowOffset, shadowColor, shadowAlpha);

        MythPoint  shadow(shadowOffset);
        shadow.NormPoint(); // scale it to screen resolution

        drawrect.setWidth(drawrect.width() + shadow.x());
        drawrect.setHeight(drawrect.height() + shadow.y());

        canvas.setWidth(canvas.width() + shadow.x());
        canvas.setHeight(canvas.height() + shadow.y());
    }

    p->SetClipRect(clipRect);
    p->DrawTextLayout(canvas, m_layouts, formats,
                      *GetFontProperties(), alpha, drawrect);
}

bool MythUIText::FormatTemplate(QString & paragraph, QTextLayout *layout)
{
    layout->clearFormats();

    FormatVector formats;
    QTextLayout::FormatRange range;
    QString fontname;
    bool    res = false;  // Return true if paragraph changed.

    if (GetFontProperties()->hasOutline())
    {
        int outlineSize = 0;
        int outlineAlpha = 255;
        QColor outlineColor;

        GetFontProperties()->GetOutline(outlineColor, outlineSize,
                                        outlineAlpha);

        outlineColor.setAlpha(outlineAlpha);

        MythPoint  outline(outlineSize, outlineSize);
        outline.NormPoint(); // scale it to screen resolution

        QPen pen;
        pen.setBrush(outlineColor);
        pen.setWidth(outline.x());

        range.start = 0;
        range.length = paragraph.size();
        range.format.setTextOutline(pen);
        formats.push_back(range);
    }

    range.start = 0;
    range.length = 0;

    int pos = 0;
    int end = 0;
    while ((pos = paragraph.indexOf("[font]", pos, Qt::CaseInsensitive)) != -1)
    {
        end = paragraph.indexOf("[/font]", pos + 1, Qt::CaseInsensitive);
        if (end != -1)
        {
            if (range.length == -1)
            {
                // End of the affected text
                range.length = pos - range.start;
                if (range.length > 0)
                {
                    formats.push_back(range);
                    LOG(VB_GUI, LOG_DEBUG,
                        QString("'%1' Setting \"%2\" with FONT %3")
                        .arg(objectName(),
                             paragraph.mid(range.start, range.length),
                             fontname));
                }
                range.length = 0;
            }

            int len = end - pos - 6;
            fontname = paragraph.mid(pos + 6, len);

            if (GetGlobalFontMap()->Contains(fontname))
            {
                MythFontProperties *fnt = GetGlobalFontMap()->GetFont(fontname);
                range.start = pos;
                range.length = -1;  // Need to find the end of the effect
                range.format.setFont(fnt->face());
                range.format.setFontStyleHint(QFont::SansSerif);
                range.format.setForeground(fnt->GetBrush());
            }
            else
            {
                LOG(VB_GUI, LOG_ERR,
                    QString("'%1' Unknown Font '%2' specified in template.")
                    .arg(objectName(), fontname));
            }

            LOG(VB_GUI, LOG_DEBUG, QString("Removing %1 through %2 '%3'")
                .arg(pos).arg(end + 7 - pos).arg(paragraph.mid(pos,
                                                               end + 7 - pos)));
            paragraph.remove(pos, end + 7 - pos);
            res = true;
        }
        else
        {
            LOG(VB_GUI, LOG_ERR,
                QString("'%1' Non-terminated [font] found in template")
                .arg(objectName()));
            break;
        }
    }

    if (range.length == -1) // To the end
    {
        range.length = paragraph.length() - range.start;
        formats.push_back(range);
        LOG(VB_GUI, LOG_DEBUG,
            QString("'%1' Setting \"%2\" with FONT %3")
            .arg(objectName(),
                 paragraph.mid(range.start, range.length),
                 fontname));
    }

    if (!formats.empty())
        layout->setFormats(formats);

    return res;
}

bool MythUIText::Layout(QString & paragraph, QTextLayout *layout, bool final,
                        bool & overflow, qreal width, qreal & height,
                        bool force, qreal & last_line_width,
                        QRectF & min_rect, int & num_lines)
{
    int last_line = 0;

    FormatTemplate(paragraph, layout);
    layout->setText(paragraph);
    layout->beginLayout();
    num_lines = 0;
    for (;;)
    {
        QTextLine line = layout->createLine();
        if (!line.isValid())
            break;

        // Try "visible" width first, so alignment works
        line.setLineWidth(width);

        if (!m_multiLine && line.textLength() < paragraph.size())
        {
            if (!force && m_cutdown != Qt::ElideNone)
            {
                QFontMetrics fm(GetFontProperties()->face());
                paragraph = fm.elidedText(paragraph, m_cutdown,
                                          width - fm.averageCharWidth());
                return false;
            }
            // If text does not fit, then expand so canvas size is correct
            line.setLineWidth(INT_MAX);
        }

        height += m_leading;
        line.setPosition(QPointF(0, height));
        height += m_lineHeight;
        if (!overflow)
        {
            if (height > m_area.height())
            {
                LOG(VB_GUI, num_lines ? LOG_DEBUG : LOG_NOTICE,
                    QString("'%1' (%2): height overflow. line height %3 "
                            "paragraph height %4, area height %5")
                    .arg(objectName(),
                         GetXMLLocation())
                    .arg(line.height())
                    .arg(height)
                    .arg(m_area.height()));

                if (!m_multiLine)
                    m_drawRect.setHeight(height);
                if (m_cutdown != Qt::ElideNone)
                {
                    QFontMetrics fm(GetFontProperties()->face());
                    QString cut_line = fm.elidedText
                                       (paragraph.mid(last_line),
                                        Qt::ElideRight,
                                        width - fm.averageCharWidth());
                    paragraph = paragraph.left(last_line) + cut_line;
                    if (last_line == 0)
                        min_rect |= line.naturalTextRect();
                    return false;
                }
                overflow = true;
            }
            else
            {
                m_drawRect.setHeight(height);
            }
            if (!m_multiLine)
                overflow = true;
        }

        last_line = line.textStart();
        last_line_width = line.naturalTextWidth();
        min_rect |= line.naturalTextRect();
        ++num_lines;

        if (final && line.textLength())
        {
        /**
         * FontMetrics::width() returns a value that is good for spacing
         * the characters, but may not represent the *full* width.  We need
         * to make sure we have enough space for the *full* width or
         * characters could be clipped.
         *
         * bearing will be negative if the char 'leans' over the "width"
         */
            QFontMetrics fm(GetFontProperties()->face());

            int bearing = fm.leftBearing(m_cutMessage[last_line]);
            m_leftBearing = std::min(m_leftBearing, bearing);
            bearing = fm.rightBearing
                      (m_cutMessage[last_line + line.textLength() - 1]);
            m_rightBearing = std::min(m_rightBearing, bearing);
        }
    }

    layout->endLayout();
    return true;
}

bool MythUIText::LayoutParagraphs(const QStringList & paragraphs,
                                  const QTextOption & textoption,
                                  qreal width, qreal & height,
                                  QRectF & min_rect, qreal & last_line_width,
                                  int & num_lines, bool final)
{
    QStringList::const_iterator Ipara;
    bool    overflow = false;
    int     idx = 0;

    for (auto *layout : std::as_const(m_layouts))
        layout->clearLayout();

    for (Ipara = paragraphs.begin(), idx = 0;
         Ipara != paragraphs.end(); ++Ipara, ++idx)
    {
        QTextLayout *layout = m_layouts[idx];
        layout->setTextOption(textoption);
        layout->setFont(m_font->face());

        QString para = *Ipara;
        qreal saved_height = height;
        QRectF saved_rect = min_rect;
        if (!Layout(para, layout, final, overflow, width, height, false,
                    last_line_width, min_rect, num_lines))
        {
            // Again, with cut down
            min_rect = saved_rect;
            height = saved_height;
            Layout(para, layout, final, overflow, width, height, true,
                   last_line_width, min_rect, num_lines);
            break;
        }
    }
    m_drawRect.setWidth(width);

    return (!overflow);
}

bool MythUIText::GetNarrowWidth(const QStringList & paragraphs,
                                const QTextOption & textoption, qreal & width)
{
    qreal    last_line_width = __builtin_nan("");
    int      last_width = -1;
    int      num_lines = 0;
    Qt::TextElideMode cutdown = m_cutdown;
    m_cutdown = Qt::ElideNone;

    int line_height = m_leading + m_lineHeight;
    width = m_area.width() / 2.0;
    int best_width = m_area.width();
    int too_narrow = 0;

    for (int attempt = 0; attempt < 10; ++attempt)
    {
        QRectF min_rect;

        m_drawRect.setWidth(0);
        qreal height = 0;

        LayoutParagraphs(paragraphs, textoption, width, height,
                         min_rect, last_line_width, num_lines, false);

        if (num_lines <= 0)
            return false;

        if (height > m_drawRect.height())
        {
            too_narrow = std::max<qreal>(too_narrow, width);

            // Too narrow?  How many lines didn't fit?
            qreal lines = round((height - m_drawRect.height()) / line_height);
            lines -= (1.0 - (last_line_width / width));
            width += (lines * width) /
                ((double)m_drawRect.height() / line_height);

            if (width > best_width || static_cast<int>(width) == last_width)
            {
                width = best_width;
                m_cutdown = cutdown;
                return true;
            }
        }
        else
        {
            best_width = std::min<qreal>(best_width, width);

            qreal lines = floor((m_area.height() - height) / line_height);
            if (lines >= 1)
            {
                // Too wide?
                width -= width * (lines / num_lines - 1 + lines);
                if (static_cast<int>(width) == last_width)
                {
                    m_cutdown = cutdown;
                    return true;
                }
            }
            else if (last_line_width < m_area.width())
            {
                // Is the last line fully used?
                width -= (1.0 - last_line_width / width) / num_lines;
                width = std::min(width, last_line_width);
                if (static_cast<int>(width) == last_width)
                {
                    m_cutdown = cutdown;
                    return true;
                }
            }
            width = std::max<qreal>(width, too_narrow);
        }
        last_width = width;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("'%1' (%2) GetNarrowWidth: Gave up "
                                     "while trying to find optimal width "
                                     "for '%3'.")
        .arg(objectName(), GetXMLLocation(), m_cutMessage));

    width = best_width;
    m_cutdown = cutdown;
    return false;
}

void MythUIText::FillCutMessage(void)
{
    if (m_area.isNull())
        return;

    QRectF min_rect;
    QFontMetrics fm(GetFontProperties()->face());

    m_lineHeight = fm.height();
    m_leading = m_multiLine ? fm.leading() + m_extraLeading : m_extraLeading;
    m_cutMessage.clear();
    m_textCursor = -1;

    if (m_message != m_defaultMessage)
    {
        bool isNumber = false;
        int value = m_message.toInt(&isNumber);

        if (isNumber && m_templateText.contains("%n"))
        {
            m_cutMessage = QCoreApplication::translate("ThemeUI",
                                           m_templateText.toUtf8(), nullptr,
                                           qAbs(value));
        }
        else if (m_templateText.contains("%1"))
        {
            QString tmp = QCoreApplication::translate("ThemeUI", m_templateText.toUtf8());
            m_cutMessage = tmp.arg(m_message);
        }
    }

    if (m_cutMessage.isEmpty())
        m_cutMessage = m_message;

    if (m_cutMessage.isEmpty())
    {
        if (m_layouts.empty())
            m_layouts.push_back(new QTextLayout);

        QTextLine line;
        QTextOption textoption(static_cast<Qt::Alignment>(m_justification));
        QVector<QTextLayout *>::iterator Ilayout = m_layouts.begin();

        (*Ilayout)->setTextOption(textoption);
        (*Ilayout)->setText("");
        (*Ilayout)->beginLayout();
        line = (*Ilayout)->createLine();
        line.setLineWidth(m_area.width());
        line.setPosition(QPointF(0, 0));
        (*Ilayout)->endLayout();
        m_drawRect.setWidth(m_area.width());
        m_drawRect.setHeight(m_lineHeight);

        for (++Ilayout ; Ilayout != m_layouts.end(); ++Ilayout)
            (*Ilayout)->clearLayout();

        m_ascent = m_descent = m_leftBearing = m_rightBearing = 0;
    }
    else
    {
        QStringList templist;
        QStringList::iterator it;

        switch (m_textCase)
        {
          case CaseUpper :
            m_cutMessage = m_cutMessage.toUpper();
            break;
          case CaseLower :
            m_cutMessage = m_cutMessage.toLower();
            break;
          case CaseCapitaliseFirst :
            //m_cutMessage = m_cutMessage.toLower();
            templist = m_cutMessage.split(". ");

            for (it = templist.begin(); it != templist.end(); ++it)
                if (!(*it).isEmpty())
                    (*it).replace(0, 1, (*it).at(0).toUpper());

            m_cutMessage = templist.join(". ");
            break;
          case CaseCapitaliseAll :
            //m_cutMessage = m_cutMessage.toLower();
            templist = m_cutMessage.split(" ");

            for (it = templist.begin(); it != templist.end(); ++it)
                if (!(*it).isEmpty())
                    (*it).replace(0, 1, (*it).at(0).toUpper());

            m_cutMessage = templist.join(" ");
            break;
            case CaseNormal:
                break;
        }

        QTextOption textoption(static_cast<Qt::Alignment>(m_justification));
        textoption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

        QStringList paragraphs = m_cutMessage.split('\n', Qt::KeepEmptyParts);
        for (int idx = m_layouts.size(); idx < paragraphs.size(); ++idx)
            m_layouts.push_back(new QTextLayout);

        qreal width = __builtin_nan("");
        if (m_multiLine && m_shrinkNarrow &&
            m_minSize.isValid() && !m_cutMessage.isEmpty())
            GetNarrowWidth(paragraphs, textoption, width);
        else
            width = m_area.width();

        qreal height = 0;
        m_leftBearing = m_rightBearing = 0;
        int   num_lines = 0;
        qreal last_line_width = __builtin_nan("");
        LayoutParagraphs(paragraphs, textoption, width, height,
                         min_rect, last_line_width, num_lines, true);

        m_canvas.setRect(0, 0, min_rect.x() + min_rect.width(), height);
        m_scrollPause = m_scrollStartDelay; // ????
        m_scrollBounce = false;

        /**
         * FontMetrics::height() returns a value that is good for spacing
         * the lines, but may not represent the *full* height.  We need
         * to make sure we have enough space for the *full* height or
         * characters could be clipped.
         */
        QRect actual = fm.boundingRect(m_cutMessage);
        m_ascent = -(actual.y() + fm.ascent());
        m_descent = actual.height() - fm.height();
    }

    if (m_scrolling)
    {
        if (m_scrollDirection == ScrollLeft ||
            m_scrollDirection == ScrollRight ||
            m_scrollDirection == ScrollHorizontal)
        {
            if (m_canvas.width() > m_drawRect.width())
            {
                m_drawRect.setX(m_area.x());
                m_drawRect.setWidth(m_area.width());
                m_scrollOffset = m_drawRect.x() - m_canvas.x();
            }
        }
        else
        {
            if (m_canvas.height() > m_drawRect.height())
            {
                m_drawRect.setY(m_area.y());
                m_drawRect.setHeight(m_area.height());
                m_scrollOffset = m_drawRect.y() - m_canvas.y();
            }
        }
    }

    // If any of hcenter|vcenter|Justify, center it all, then adjust
    if ((m_justification & (Qt::AlignCenter|Qt::AlignJustify)) != 0U)
    {
        m_drawRect.moveCenter(m_area.center());
        min_rect.moveCenter(m_area.center());
    }

    // Adjust horizontal
    if (m_justification & Qt::AlignLeft)
    {
        // If text size is less than allowed min size, center it
        if (m_shrinkNarrow && m_minSize.isValid() && min_rect.isValid() &&
            min_rect.width() < m_minSize.x())
        {
            m_drawRect.moveLeft(m_area.x() +
                                (((m_minSize.x() - min_rect.width() +
                                   fm.averageCharWidth()) / 2)));
            min_rect.setWidth(m_minSize.x());
        }
        else
        {
            m_drawRect.moveLeft(m_area.x());
        }

        min_rect.moveLeft(m_area.x());
    }
    else if (m_justification & Qt::AlignRight)
    {
        // If text size is less than allowed min size, center it
        if (m_shrinkNarrow && m_minSize.isValid() && min_rect.isValid() &&
            min_rect.width() < m_minSize.x())
        {
            m_drawRect.moveRight(m_area.x() + m_area.width() -
                                (((m_minSize.x() - min_rect.width() +
                                   fm.averageCharWidth()) / 2)));
            min_rect.setWidth(m_minSize.x());
        }
        else
        {
            m_drawRect.moveRight(m_area.x() + m_area.width());
        }

        min_rect.moveRight(m_area.x() + m_area.width());
    }

    // Adjust vertical
    if (m_justification & Qt::AlignTop)
    {
        // If text size is less than allowed min size, center it
        if (!m_shrinkNarrow && m_minSize.isValid() && min_rect.isValid() &&
            min_rect.height() < m_minSize.y())
        {
            m_drawRect.moveTop(m_area.y() +
                               ((m_minSize.y() - min_rect.height()) / 2));
            min_rect.setHeight(m_minSize.y());
        }
        else
        {
            m_drawRect.moveTop(m_area.y());
        }

        min_rect.moveTop(m_area.y());
    }
    else if (m_justification & Qt::AlignBottom)
    {
        // If text size is less than allowed min size, center it
        if (!m_shrinkNarrow && m_minSize.isValid() && min_rect.isValid() &&
            min_rect.height() < m_minSize.y())
        {
            m_drawRect.moveBottom(m_area.y() + m_area.height() -
                                  ((m_minSize.y() - min_rect.height()) / 2));
            min_rect.setHeight(m_minSize.y());
        }
        else
        {
            m_drawRect.moveBottom(m_area.y() + m_area.height());
        }

        min_rect.moveBottom(m_area.y() + m_area.height());
    }

    m_initiator = m_enableInitiator;
    if (m_minSize.isValid())
    {
        // Record the minimal area needed for the message.
        SetMinArea(MythRect(min_rect.toRect()));
    }
}

int MythUIText::MoveCursor(int lines)
{
    int lineNo = -1;
    int lineCount = 0;
    int currPos = 0;
    int layoutStartPos = 0;
    int xPos = 0;

    for (int x = 0; x < m_layouts.count(); x++)
    {
        QTextLayout *layout = m_layouts.at(x);

        for (int y = 0; y < layout->lineCount(); y++)
        {
            lineCount++;
            if (lineNo != -1)
                continue;

            QTextLine line = layout->lineAt(y);

            if (m_textCursor >= currPos && m_textCursor < currPos + line.textLength()
                + (line.lineNumber() == layout->lineCount() - 1 ? 1 : 0))
            {
                lineNo = lineCount - 1;
                xPos = line.cursorToX(m_textCursor - layoutStartPos);
                continue;
            }

            currPos += line.textLength();
        }

        currPos += 1; // skip the newline
        layoutStartPos = currPos;
    }

    // are we are at the top and need to scroll up
    if (lineNo == 0 && lines < 0)
        return -1;

    // are we at the bottom and need to scroll down
    if (lineNo == lineCount - 1 && lines > 0)
        return -1;

    if (lineNo == -1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("'%1' (%2) MoveCursor offset %3 not found in ANY paragraph!")
            .arg(objectName(), GetXMLLocation(), QString::number(m_textCursor)));
        return m_textCursor;
    }

    int newLine = lineNo + lines;

    newLine = std::max(newLine, 0);

    if (newLine >= lineCount)
        newLine = lineCount - 1;

    lineNo = -1;
    layoutStartPos = 0;

    for (int x = 0; x < m_layouts.count(); x++)
    {
        QTextLayout *layout = m_layouts.at(x);

        for (int y = 0; y < layout->lineCount(); y++)
        {
            lineNo++;
            QTextLine line = layout->lineAt(y);

            if (lineNo == newLine)
                return layoutStartPos + line.xToCursor(xPos);
        }

        layoutStartPos += layout->text().length() + 1;
    }

    // should never reach here
    return m_textCursor;
}

QPoint MythUIText::CursorPosition(int text_offset)
{
    if (m_layouts.empty())
        return m_area.topLeft().toQPoint();

    if (text_offset == m_textCursor)
        return m_cursorPos;
    m_textCursor = text_offset;

    QVector<QTextLayout *>::const_iterator Ipara {};
    QPoint pos;
    int    x = 0;
    int    y = 0;
    int    offset = text_offset;

    for (Ipara = m_layouts.constBegin(); Ipara != m_layouts.constEnd(); ++Ipara)
    {
        QTextLine line = (*Ipara)->lineForTextPosition(offset);

        if (line.isValid())
        {
            pos.setX(line.cursorToX(&offset));
            pos.setY(line.y());
            break;
        }
        offset -= ((*Ipara)->text().size() + 1); // Account for \n
    }
    if (Ipara == m_layouts.constEnd())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("'%1' (%2) CursorPosition offset %3 not found in "
                    "ANY paragraph!")
            .arg(objectName(), GetXMLLocation(), QString::number(text_offset)));
        return m_area.topLeft().toQPoint();
    }

    int mid = m_drawRect.width() / 2;
    if (m_canvas.width() <= m_drawRect.width() || pos.x() <= mid)
        x = 0;  // start
    else if (pos.x() >= m_canvas.width() - mid) // end
    {
        x = m_canvas.width() - m_drawRect.width();
        pos.setX(pos.x() - x);
    }
    else // middle
    {
        x = pos.x() - mid;
        pos.setX(pos.x() - x);
    }

    int line_height = m_lineHeight + m_leading;
    mid = m_area.height() / 2;
    mid -= (mid % line_height);
    y = pos.y() - mid;

    if (y <= 0 || m_canvas.height() <= m_area.height()) // Top of buffer
        y = 0;
    else if (y + m_area.height() > m_canvas.height()) // Bottom of buffer
    {
        int visible_lines = ((m_area.height() / line_height) * line_height);
        y = m_canvas.height() - visible_lines;
        pos.setY(visible_lines - (m_canvas.height() - pos.y()));
    }
    else // somewhere in the middle
    {
        y -= m_leading;
        pos.setY(mid + m_leading);
    }

    m_canvas.moveTopLeft(QPoint(-x, -y));

    // Compensate for vertical alignment
    pos.setY(pos.y() + m_drawRect.y() - m_area.y());

    pos += m_area.topLeft().toQPoint();
    m_cursorPos = pos;

    return pos;
}

void MythUIText::Pulse(void)
{
    //
    //  Call out base class pulse which will handle any alpha cycling and
    //  movement
    //

    MythUIType::Pulse();

    // Calculate interval since last update in msecs - clamped to 50msecs (20Hz)
    // in case of UI blocking/pauses
    int64_t currentmsecs = QDateTime::currentMSecsSinceEpoch();
    int64_t interval = std::min(currentmsecs - m_lastUpdate, static_cast<int64_t>(50));
    m_lastUpdate = currentmsecs;

    // Rates are pixels per second (float) normalised to 70Hz for historical reasons.
    // Convert interval accordingly.
    float rate = (interval / 1000.0F) * DEFAULT_REFRESH_RATE;

    if (m_colorCycling)
    {
        m_curR += m_incR;
        m_curG += m_incG;
        m_curB += m_incB;

        m_curStep++;

        if (m_curStep >= m_numSteps)
        {
            m_curStep = 0;
            m_incR *= -1;
            m_incG *= -1;
            m_incB *= -1;
        }

        QColor newColor = QColor(static_cast<int>(m_curR),
                                 static_cast<int>(m_curG),
                                 static_cast<int>(m_curB));
        if (newColor != m_font->color())
        {
            m_font->SetColor(newColor);
            SetRedraw();
        }
    }

    if (m_scrolling)
    {
        if (m_scrollPause > 0.0F)
            m_scrollPause -= rate;
        else
        {
            if (m_scrollBounce)
                m_scrollPos += m_scrollReturnRate * rate;
            else
                m_scrollPos += m_scrollForwardRate * rate;
        }

        int whole = static_cast<int>(m_scrollPos);
        if (m_scrollPosWhole != whole)
        {
            int shift = whole - m_scrollPosWhole;
            m_scrollPosWhole = whole;

            switch (m_scrollDirection)
            {
              case ScrollLeft :
                if (m_canvas.width() > m_drawRect.width())
                {
                    ShiftCanvas(-shift, 0);
                    if (m_canvas.x() + m_canvas.width() < 0)
                    {
                        SetCanvasPosition(m_drawRect.width(), 0);
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                }
                break;
              case ScrollRight :
                if (m_canvas.width() > m_drawRect.width())
                {
                    ShiftCanvas(shift, 0);
                    if (m_canvas.x() > m_drawRect.width())
                    {
                        SetCanvasPosition(-m_canvas.width(), 0);
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                }
                break;
              case ScrollHorizontal:
                if (m_canvas.width() <= m_drawRect.width())
                    break;
                if (m_scrollBounce) // scroll right
                {
                    if (m_canvas.x() + m_scrollOffset > m_drawRect.x())
                    {
                        m_scrollBounce = false;
                        m_scrollPause = m_scrollStartDelay;
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                    else
                    {
                        ShiftCanvas(shift, 0);
                    }
                }
                else // scroll left
                {
                    if (m_canvas.x() + m_canvas.width() + m_scrollOffset <
                        m_drawRect.x() + m_drawRect.width())
                    {
                        m_scrollBounce = true;
                        m_scrollPause = m_scrollReturnDelay;
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                    else
                    {
                        ShiftCanvas(-shift, 0);
                    }
                }
                break;
              case ScrollUp :
                if (m_canvas.height() > m_drawRect.height())
                {
                    ShiftCanvas(0, -shift);
                    if (m_canvas.y() + m_canvas.height() < 0)
                    {
                        SetCanvasPosition(0, m_drawRect.height());
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                }
                break;
              case ScrollDown :
                if (m_canvas.height() > m_drawRect.height())
                {
                    ShiftCanvas(0, shift);
                    if (m_canvas.y() > m_drawRect.height())
                    {
                        SetCanvasPosition(0, -m_canvas.height());
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                }
                break;
              case ScrollVertical:
                if (m_canvas.height() <= m_drawRect.height())
                    break;
                if (m_scrollBounce) // scroll down
                {
                    if (m_canvas.y() + m_scrollOffset > m_drawRect.y())
                    {
                        m_scrollBounce = false;
                        m_scrollPause = m_scrollStartDelay;
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                    else
                    {
                        ShiftCanvas(0, shift);
                    }
                }
                else // scroll up
                {
                    if (m_canvas.y() + m_canvas.height() + m_scrollOffset <
                        m_drawRect.y() + m_drawRect.height())
                    {
                        m_scrollBounce = true;
                        m_scrollPause = m_scrollReturnDelay;
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                    else
                    {
                        ShiftCanvas(0, -shift);
                    }
                }
                break;
              case ScrollNone:
                break;
            }
        }
    }
}

void MythUIText::CycleColor(const QColor& startColor, const QColor& endColor, int numSteps)
{
    if (!GetPainter()->SupportsAnimation())
        return;

    m_startColor = startColor;
    m_endColor = endColor;
    m_numSteps = numSteps;
    m_curStep = 0;

    m_curR = startColor.red();
    m_curG = startColor.green();
    m_curB = startColor.blue();

    m_incR = (endColor.red()   * 1.0F - m_curR) / m_numSteps;
    m_incG = (endColor.green() * 1.0F - m_curG) / m_numSteps;
    m_incB = (endColor.blue()  * 1.0F - m_curB) / m_numSteps;

    m_colorCycling = true;
}

void MythUIText::StopCycling(void)
{
    if (!m_colorCycling)
        return;

    m_font->SetColor(m_startColor);
    m_colorCycling = false;
    SetRedraw();
}

bool MythUIText::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
        m_origDisplayRect = m_area;
    }
    //    else if (element.tagName() == "altarea") // Unused, but maybe in future?
    //        m_altDisplayRect = parseRect(element);
    else if (element.tagName() == "font")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *fp = GetFont(fontname);

        if (!fp)
            fp = GetGlobalFontMap()->GetFont(fontname);

        if (fp)
        {
            MythFontProperties font = *fp;
            MythMainWindow* window = GetMythMainWindow();
            font.Rescale(window->GetUIScreenRect().height());
            font.AdjustStretch(window->GetFontStretch());
            QString state = element.attribute("state", "");

            if (!state.isEmpty())
            {
                m_fontStates.insert(state, font);
            }
            else
            {
                m_fontStates.insert("default", font);
                *m_font = m_fontStates["default"];
            }
        }
    }
    else if (element.tagName() == "extraleading")
    {
        m_extraLeading = getFirstText(element).toInt();
    }
    else if (element.tagName() == "value")
    {
        if (element.attribute("lang", "").isEmpty())
        {
            m_message = QCoreApplication::translate("ThemeUI",
                                        parseText(element).toUtf8());
        }
        else if ((element.attribute("lang", "").toLower() ==
                  gCoreContext->GetLanguageAndVariant()) ||
                 (element.attribute("lang", "").toLower() ==
                  gCoreContext->GetLanguage()))
        {
            m_message = parseText(element);
        }

        m_defaultMessage = m_message;
        SetText(m_message);
    }
    else if (element.tagName() == "template")
    {
        m_templateText = parseText(element);
    }
    else if (element.tagName() == "cutdown")
    {
        QString mode = getFirstText(element).toLower();

        if (mode == "left")
            SetCutDown(Qt::ElideLeft);
        else if (mode == "middle")
            SetCutDown(Qt::ElideMiddle);
        else if (mode == "right" || parseBool(element))
            SetCutDown(Qt::ElideRight);
        else
            SetCutDown(Qt::ElideNone);
    }
    else if (element.tagName() == "multiline")
    {
        SetMultiLine(parseBool(element));
    }
    else if (element.tagName() == "align")
    {
        QString align = getFirstText(element).toLower();
        SetJustification(parseAlignment(align));
    }
    else if (element.tagName() == "colorcycle")
    {
        if (GetPainter()->SupportsAnimation())
        {
            QString tmp = element.attribute("start");

            if (!tmp.isEmpty())
                m_startColor = QColor(tmp);

            tmp = element.attribute("end");

            if (!tmp.isEmpty())
                m_endColor = QColor(tmp);

            tmp = element.attribute("steps");

            if (!tmp.isEmpty())
                m_numSteps = tmp.toInt();

            // initialize the rest of the stuff
            CycleColor(m_startColor, m_endColor, m_numSteps);
        }
        else
        {
            m_colorCycling = false;
        }

        m_colorCycling = parseBool(element.attribute("disable"));
    }
    else if (element.tagName() == "scroll")
    {
        if (GetPainter()->SupportsAnimation())
        {
            QString tmp = element.attribute("direction");

            if (!tmp.isEmpty())
            {
                tmp = tmp.toLower();

                if (tmp == "left")
                    m_scrollDirection = ScrollLeft;
                else if (tmp == "right")
                    m_scrollDirection = ScrollRight;
                else if (tmp == "up")
                    m_scrollDirection = ScrollUp;
                else if (tmp == "down")
                    m_scrollDirection = ScrollDown;
                else if (tmp == "horizontal")
                    m_scrollDirection = ScrollHorizontal;
                else if (tmp == "vertical")
                    m_scrollDirection = ScrollVertical;
                else
                {
                    m_scrollDirection = ScrollNone;
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("'%1' (%2) Invalid scroll attribute")
                        .arg(objectName(), GetXMLLocation()));
                }
            }

            tmp = element.attribute("startdelay");
            if (!tmp.isEmpty())
            {
                float seconds = tmp.toFloat();
                m_scrollStartDelay = static_cast<int>(lroundf(seconds * DEFAULT_REFRESH_RATE));
            }
            tmp = element.attribute("returndelay");
            if (!tmp.isEmpty())
            {
                float seconds = tmp.toFloat();
                m_scrollReturnDelay = static_cast<int>(lroundf(seconds * DEFAULT_REFRESH_RATE));
            }
            tmp = element.attribute("rate");
            if (!tmp.isEmpty())
            {
#if 0 // scroll rate as a percentage of 70Hz
                float percent = tmp.toFloat() / 100.0;
                m_scrollForwardRate = percent;
#else // scroll rate as pixels per second
                int pixels = tmp.toInt();
                m_scrollForwardRate = pixels / static_cast<float>(DEFAULT_REFRESH_RATE);
#endif
            }
            tmp = element.attribute("returnrate");
            if (!tmp.isEmpty())
            {
#if 0 // scroll rate as a percentage of 70Hz
                float percent = tmp.toFloat() / 100.0;
                m_scrollReturnRate = percent;
#else // scroll rate as pixels per second
                int pixels = tmp.toInt();
                m_scrollReturnRate = pixels / static_cast<float>(DEFAULT_REFRESH_RATE);
#endif
            }

            m_scrolling = true;
        }
        else
        {
            m_scrolling = false;
        }
    }
    else if (element.tagName() == "case")
    {
        QString stringCase = getFirstText(element).toLower();

        if (stringCase == "lower")
            m_textCase = CaseLower;
        else if (stringCase == "upper")
            m_textCase = CaseUpper;
        else if (stringCase == "capitalisefirst")
            m_textCase = CaseCapitaliseFirst;
        else  if (stringCase == "capitaliseall")
            m_textCase = CaseCapitaliseAll;
        else
            m_textCase = CaseNormal;
    }
    else
    {
        if (element.tagName() == "minsize" && element.hasAttribute("shrink"))
        {
            m_shrinkNarrow = (element.attribute("shrink")
                              .toLower() != "short");
        }

        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

void MythUIText::CopyFrom(MythUIType *base)
{
    auto *text = dynamic_cast<MythUIText *>(base);

    if (!text)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("'%1' (%2) ERROR, bad parsing '%3' (%4)")
            .arg(objectName(), GetXMLLocation(),
                 base->objectName(), base->GetXMLLocation()));
        return;
    }

    m_justification = text->m_justification;
    m_origDisplayRect = text->m_origDisplayRect;
    m_altDisplayRect = text->m_altDisplayRect;
    m_canvas = text->m_canvas;
    m_drawRect = text->m_drawRect;

    m_defaultMessage = text->m_defaultMessage;
    SetText(text->m_message);
    m_cutMessage = text->m_cutMessage;
    m_templateText = text->m_templateText;

    m_shrinkNarrow = text->m_shrinkNarrow;
    m_cutdown = text->m_cutdown;
    m_multiLine = text->m_multiLine;
    m_leading = text->m_leading;
    m_extraLeading = text->m_extraLeading;
    m_lineHeight = text->m_lineHeight;
    m_textCursor = text->m_textCursor;

    QMutableMapIterator<QString, MythFontProperties> it(text->m_fontStates);

    while (it.hasNext())
    {
        it.next();
        m_fontStates.insert(it.key(), it.value());
    }

    *m_font = m_fontStates["default"];

    m_colorCycling = text->m_colorCycling;
    m_startColor = text->m_startColor;
    m_endColor = text->m_endColor;
    m_numSteps = text->m_numSteps;
    m_curStep = text->m_curStep;
    m_curR = text->m_curR;
    m_curG = text->m_curG;
    m_curB = text->m_curB;
    m_incR = text->m_incR;
    m_incG = text->m_incG;
    m_incB = text->m_incB;

    m_scrollStartDelay = text->m_scrollStartDelay;
    m_scrollReturnDelay = text->m_scrollReturnDelay;
    m_scrollForwardRate = text->m_scrollForwardRate;
    m_scrollReturnRate = text->m_scrollReturnRate;
    m_scrollDirection = text->m_scrollDirection;
    m_scrolling = text->m_scrolling;

    m_textCase = text->m_textCase;

    MythUIType::CopyFrom(base);
    FillCutMessage();
}

void MythUIText::CreateCopy(MythUIType *parent)
{
    auto *text = new MythUIText(parent, objectName());
    text->CopyFrom(this);
}


void MythUIText::Finalize(void)
{
    if (m_scrolling && m_cutdown != Qt::ElideNone)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("'%1' (%2): <scroll> and <cutdown> are not combinable.")
            .arg(objectName(), GetXMLLocation()));
        m_cutdown = Qt::ElideNone;
    }
    FillCutMessage();
}
