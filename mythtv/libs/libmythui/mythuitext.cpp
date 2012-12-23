
#include "mythuitext.h"

#include <QCoreApplication>
#include <QtGlobal>
#include <QDomDocument>
#include <QFontMetrics>
#include <QString>
#include <QHash>

#include "mythlogging.h"

#include "mythuihelper.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythcorecontext.h"

#include "compat.h"

MythUIText::MythUIText(MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_Justification(Qt::AlignLeft | Qt::AlignTop), m_OrigDisplayRect(),
      m_AltDisplayRect(),                            m_Canvas(),
      m_drawRect(),                                  m_cursorPos(-1, -1),
      m_Message(""),                                 m_CutMessage(""),
      m_DefaultMessage(""),                          m_TemplateText(""),
      m_ShrinkNarrow(true),                          m_Cutdown(Qt::ElideRight),
      m_MultiLine(false),                            m_Ascent(0),
      m_Descent(0),                                  m_leftBearing(0),
      m_rightBearing(0),                             m_Leading(1),
      m_extraLeading(0),                             m_lineHeight(0),
      m_textCursor(-1),
      m_Font(new MythFontProperties()),              m_colorCycling(false),
      m_startColor(),                                m_endColor(),
      m_numSteps(0),                                 m_curStep(0),
      curR(0.0),              curG(0.0),             curB(0.0),
      incR(0.0),              incG(0.0),             incB(0.0),
      m_scrollStartDelay(ScrollBounceDelay),
      m_scrollReturnDelay(ScrollBounceDelay),        m_scrollPause(0),
      m_scrollForwardRate(70.0 / MythMainWindow::drawRefresh),
      m_scrollReturnRate(70.0 / MythMainWindow::drawRefresh),
      m_scrollBounce(false),                         m_scrollOffset(0),
      m_scrollPos(0),                                m_scrollPosWhole(0),
      m_scrollDirection(ScrollNone),                 m_scrolling(false),
      m_textCase(CaseNormal)
{
#if 0 // Not currently used
    m_usingAltArea = false;
#endif
    m_EnableInitiator = true;

    m_FontStates.insert("default", MythFontProperties());
    *m_Font = m_FontStates["default"];
}

MythUIText::MythUIText(const QString &text, const MythFontProperties &font,
                       QRect displayRect, QRect altDisplayRect,
                       MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_Justification(Qt::AlignLeft | Qt::AlignTop),
      m_OrigDisplayRect(displayRect), m_AltDisplayRect(altDisplayRect),
      m_Canvas(0, 0, displayRect.width(), displayRect.height()),
      m_drawRect(displayRect),        m_cursorPos(-1, -1),
      m_Message(text.trimmed()),
      m_CutMessage(""),               m_DefaultMessage(text),
      m_Cutdown(Qt::ElideRight),      m_Font(new MythFontProperties()),
      m_colorCycling(false),          m_startColor(),
      m_endColor(),                   m_numSteps(0),
      m_curStep(0),
      curR(0.0),      curG(0.0),      curB(0.0),
      incR(0.0),      incG(0.0),      incB(0.0)
{
#if 0 // Not currently used
    m_usingAltArea = false;
#endif
    m_ShrinkNarrow = true;
    m_MultiLine = false;

    m_scrollStartDelay = m_scrollReturnDelay = ScrollBounceDelay;
    m_scrollPause = 0;
    m_scrollForwardRate = m_scrollReturnRate =
                          70.0 / MythMainWindow::drawRefresh;
    m_scrollBounce = false;
    m_scrollOffset = 0;
    m_scrollPos = 0;
    m_scrollPosWhole = 0;
    m_scrollDirection = ScrollNone;
    m_scrolling = false;

    m_textCase = CaseNormal;
    m_Ascent = m_Descent = m_leftBearing = m_rightBearing = 0;
    m_Leading = 1;
    m_extraLeading = 0;
    m_lineHeight = 0;
    m_textCursor = -1;
    m_EnableInitiator = true;

    SetArea(displayRect);
    m_FontStates.insert("default", font);
    *m_Font = m_FontStates["default"];
}

MythUIText::~MythUIText()
{
    delete m_Font;
    m_Font = NULL;

    QVector<QTextLayout *>::iterator Ilayout;
    for (Ilayout = m_Layouts.begin(); Ilayout != m_Layouts.end(); ++Ilayout)
        delete *Ilayout;
}

void MythUIText::Reset()
{
    if (m_Message != m_DefaultMessage)
    {
        SetText(m_DefaultMessage);
        SetRedraw();
        emit DependChanged(true);
    }

    SetFontState("default");

    MythUIType::Reset();
}

void MythUIText::SetText(const QString &text)
{
    QString newtext = text;

    if (!m_Layouts.isEmpty() && newtext == m_Message)
        return;

    if (newtext.isEmpty())
    {
        m_Message = m_DefaultMessage;
        emit DependChanged(true);
    }
    else
    {
        m_Message = newtext;
        emit DependChanged(false);
    }
    m_CutMessage.clear();
    FillCutMessage();

    SetRedraw();
}

void MythUIText::SetTextFromMap(QHash<QString, QString> &map)
{
    if (!IsVisible())
        return;

    if (map.contains(objectName()))
    {
        QString newText = GetTemplateText();

        if (newText.isEmpty())
            newText = GetDefaultText();

        QRegExp regexp("%(([^\\|%]+)?\\||\\|(.))?(\\w+)(\\|(.+))?%");
        regexp.setMinimal(true);

        if (!newText.isEmpty() && newText.contains(regexp))
        {
            int pos = 0;

            QString translatedTemplate = qApp->translate("ThemeUI",
                                                         newText.toUtf8(),
                                                         NULL,
                                                 QCoreApplication::UnicodeUTF8);

            QString tempString = translatedTemplate;

            while ((pos = regexp.indexIn(translatedTemplate, pos)) != -1)
            {
                QString key = regexp.cap(4).toLower().trimmed();
                QString replacement;

                if (!map.value(key).isEmpty())
                {
                    replacement = QString("%1%2%3%4")
                                  .arg(regexp.cap(2))
                                  .arg(regexp.cap(3))
                                  .arg(map.value(key))
                                  .arg(regexp.cap(6));
                }

                tempString.replace(regexp.cap(0), replacement);
                pos += regexp.matchedLength();
            }

            newText = tempString;
        }
        else
            newText = map.value(objectName());

        SetText(newText);
    }
}

QString MythUIText::GetText(void) const
{
    return m_Message;
}

QString MythUIText::GetDefaultText(void) const
{
    return m_DefaultMessage;
}

void MythUIText::SetFontProperties(const MythFontProperties &fontProps)
{
    m_FontStates.insert("default", fontProps);
    if (m_Font->GetHash() != m_FontStates["default"].GetHash())
    {
        *m_Font = m_FontStates["default"];
        if (!m_Message.isEmpty())
        {
            FillCutMessage();
            SetRedraw();
        }
    }
}

void MythUIText::SetFontState(const QString &state)
{
    if (m_FontStates.contains(state))
    {
        if (m_Font->GetHash() == m_FontStates[state].GetHash())
            return;
        *m_Font = m_FontStates[state];
    }
    else
    {
        if (m_Font->GetHash() == m_FontStates["default"].GetHash())
            return;
        *m_Font = m_FontStates["default"];
    }
    if (!m_Message.isEmpty())
    {
        FillCutMessage();
        SetRedraw();
    }
}

#if 0 // Not currently used
void MythUIText::UseAlternateArea(bool useAlt)
{
    if (useAlt && m_AltDisplayRect.width() > 1)
    {
        MythUIType::SetArea(m_AltDisplayRect);
        m_usingAltArea = true;
    }
    else
    {
        MythUIType::SetArea(m_OrigDisplayRect);
        m_usingAltArea = false;
    }

    FillCutMessage();
}
#endif

void MythUIText::SetJustification(int just)
{
    int h = just & Qt::AlignHorizontal_Mask;
    int v = just & Qt::AlignVertical_Mask;

    if ((h && (m_Justification & Qt::AlignHorizontal_Mask) ^ h) ||
        (v && (m_Justification & Qt::AlignVertical_Mask) ^ v))
    {
        // preserve the wordbreak attribute, drop everything else
        m_Justification = m_Justification & Qt::TextWordWrap;
        m_Justification |= just;
        if (!m_Message.isEmpty())
        {
            FillCutMessage();
            SetRedraw();
        }
    }
}

int MythUIText::GetJustification(void)
{
    return m_Justification;
}

void MythUIText::SetCutDown(Qt::TextElideMode mode)
{
    if (mode != m_Cutdown)
    {
        m_Cutdown = mode;
        if (m_scrolling && m_Cutdown != Qt::ElideNone)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("'%1' (%2): <scroll> and "
                                             "<cutdown> are not combinable.")
                .arg(objectName()).arg(GetXMLLocation()));
            m_Cutdown = Qt::ElideNone;
        }
        if (!m_Message.isEmpty())
        {
            FillCutMessage();
            SetRedraw();
        }
    }
}

void MythUIText::SetMultiLine(bool multiline)
{
    if (multiline != m_MultiLine)
    {
        m_MultiLine = multiline;

        if (m_MultiLine)
            m_Justification |= Qt::TextWordWrap;
        else
            m_Justification &= ~Qt::TextWordWrap;

        if (!m_Message.isEmpty())
        {
            FillCutMessage();
            SetRedraw();
        }
    }
}

void MythUIText::SetArea(const MythRect &rect)
{
    MythUIType::SetArea(rect);
    m_CutMessage.clear();

    m_drawRect = m_Area;
    FillCutMessage();
}

void MythUIText::SetPosition(const MythPoint &pos)
{
    MythUIType::SetPosition(pos);
    m_drawRect.moveTopLeft(m_Area.topLeft());
}

void MythUIText::SetCanvasPosition(int x, int y)
{
    QPoint newpoint(x, y);

    if (newpoint == m_Canvas.topLeft())
        return;

    m_Canvas.moveTopLeft(newpoint);
    SetRedraw();
}

void MythUIText::ShiftCanvas(int x, int y)
{
    if (x == 0 && y == 0)
        return;

    m_Canvas.moveTop(m_Canvas.y() + y);
    m_Canvas.moveLeft(m_Canvas.x() + x);
    SetRedraw();
}

void MythUIText::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect)
{
    if (m_Canvas.isNull())
        return;

    FormatVector formats;
    QRect drawrect = m_drawRect.toQRect();
    drawrect.translate(xoffset, yoffset);
    QRect canvas = m_Canvas.toQRect();

    int alpha = CalcAlpha(alphaMod);

    if (m_Ascent)
    {
        drawrect.setY(drawrect.y() - m_Ascent);
        canvas.moveTop(canvas.y() + m_Ascent);
        canvas.setHeight(canvas.height() + m_Ascent);
    }
    if (m_Descent)
    {
        drawrect.setHeight(drawrect.height() + m_Descent);
        canvas.setHeight(canvas.height() + m_Descent);
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
        QTextLayout::FormatRange range;

        QColor outlineColor;
        int outlineSize, outlineAlpha;

        GetFontProperties()->GetOutline(outlineColor, outlineSize,
                                        outlineAlpha);
        outlineColor.setAlpha(outlineAlpha);

        MythPoint  outline(outlineSize, outlineSize);
        outline.NormPoint(); // scale it to screen resolution

        QPen pen;
        pen.setBrush(outlineColor);
        pen.setWidth(outline.x());

        range.start = 0;
        range.length = m_CutMessage.size();
        range.format.setTextOutline(pen);
        formats.push_back(range);

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
        int    shadowAlpha;

        GetFontProperties()->GetShadow(shadowOffset, shadowColor, shadowAlpha);

        MythPoint  shadow(shadowOffset);
        shadow.NormPoint(); // scale it to screen resolution

        drawrect.setWidth(drawrect.width() + shadow.x());
        drawrect.setHeight(drawrect.height() + shadow.y());

        canvas.setWidth(canvas.width() + shadow.x());
        canvas.setHeight(canvas.height() + shadow.y());
    }

    p->DrawTextLayout(canvas, m_Layouts, formats,
                      *GetFontProperties(), alpha, drawrect);
}

bool MythUIText::Layout(QString & paragraph, QTextLayout *layout, bool final,
                        bool & overflow, qreal width, qreal & height,
                        bool force, qreal & last_line_width,
                        QRectF & min_rect, int & num_lines)
{
    int last_line = 0;

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

        if (!m_MultiLine && line.textLength() < paragraph.size())
        {
            if (!force && m_Cutdown != Qt::ElideNone)
            {
                QFontMetrics fm(GetFontProperties()->face());
                paragraph = fm.elidedText(paragraph, m_Cutdown,
                                          width - fm.averageCharWidth());
                return false;
            }
            // If text does not fit, then expand so canvas size is correct
            line.setLineWidth(INT_MAX);
        }

        height += m_Leading;
        line.setPosition(QPointF(0, height));
        height += m_lineHeight;
        if (!overflow)
        {
            if (height > m_Area.height())
            {
                LOG(VB_GUI, num_lines ? LOG_DEBUG : LOG_NOTICE,
                    QString("'%1' (%2): height overflow. line height %3 "
                            "paragraph height %4, area height %5")
                    .arg(objectName())
                    .arg(GetXMLLocation())
                    .arg(line.height())
                    .arg(height)
                    .arg(m_Area.height()));

                if (!m_MultiLine)
                    m_drawRect.setHeight(height);
                if (m_Cutdown != Qt::ElideNone)
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
                m_drawRect.setHeight(height);
            if (!m_MultiLine)
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
            int bearing;

            bearing = fm.leftBearing(m_CutMessage[last_line]);
            if (m_leftBearing > bearing)
                m_leftBearing = bearing;
            bearing = fm.rightBearing
                      (m_CutMessage[last_line + line.textLength() - 1]);
            if (m_rightBearing > bearing)
                m_rightBearing = bearing;
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
    QVector<QTextLayout *>::iterator Ilayout;
    QTextLayout *layout;
    QString para;
    bool    overflow = false;
    qreal   saved_height;
    QRectF  saved_rect;
    int     idx;

    for (Ilayout = m_Layouts.begin(); Ilayout != m_Layouts.end(); ++Ilayout)
        (*Ilayout)->clearLayout();

    for (Ipara = paragraphs.begin(), idx = 0;
         Ipara != paragraphs.end(); ++Ipara, ++idx)
    {
        layout = m_Layouts[idx];
        layout->setTextOption(textoption);
        layout->setFont(m_Font->face());

        para = *Ipara;
        saved_height = height;
        saved_rect = min_rect;
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
    qreal    height, last_line_width, lines;
    int      best_width, too_narrow, last_width = -1;
    int      num_lines, line_height = 0;
    int      attempt = 0;
    Qt::TextElideMode cutdown = m_Cutdown;
    m_Cutdown = Qt::ElideNone;

    line_height = m_Leading + m_lineHeight;
    width = m_Area.width() / 2.0;
    best_width = m_Area.width();
    too_narrow = 0;

    for (attempt = 0; attempt < 10; ++attempt)
    {
        QRectF min_rect;

        m_drawRect.setWidth(0);
        height = 0;

        LayoutParagraphs(paragraphs, textoption, width, height,
                         min_rect, last_line_width, num_lines, false);

        if (num_lines <= 0)
            return false;

        if (height > m_drawRect.height())
        {
            if (too_narrow < width)
                too_narrow = width;

            // Too narrow?  How many lines didn't fit?
            lines = static_cast<int>
                    ((height - m_drawRect.height()) / line_height);
            lines -= (1.0 - last_line_width / width);
            width += (lines * width) /
                     (m_drawRect.height() / line_height);

            if (width > best_width || static_cast<int>(width) == last_width)
            {
                width = best_width;
                m_Cutdown = cutdown;
                return true;
            }
        }
        else
        {
            if (best_width > width)
                best_width = width;

            lines = static_cast<int>
                    (m_Area.height() - height) / line_height;
            if (lines >= 1)
            {
                // Too wide?
                width -= width *
                         (lines / static_cast<qreal>(num_lines - 1 + lines));
                if (static_cast<int>(width) == last_width)
                {
                    m_Cutdown = cutdown;
                    return true;
                }
            }
            else if (last_line_width < m_Area.width())
            {
                // Is the last line fully used?
                width -= (1.0 - last_line_width / width) / num_lines;
                if (width > last_line_width)
                    width = last_line_width;
                if (static_cast<int>(width) == last_width)
                {
                    m_Cutdown = cutdown;
                    return true;
                }
            }
            if (width < too_narrow)
                width = too_narrow;
        }
        last_width = width;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("'%1' (%2) GetNarrowWidth: Gave up "
                                     "while trying to find optimal width "
                                     "for '%3'.")
        .arg(objectName()).arg(GetXMLLocation()).arg(m_CutMessage));

    width = best_width;
    m_Cutdown = cutdown;
    return false;
}

void MythUIText::FillCutMessage(void)
{
    if (m_Area.isNull())
        return;

    qreal  width, height;
    QRectF min_rect;
    QFontMetrics fm(GetFontProperties()->face());

    m_lineHeight = fm.height();
    m_Leading = m_MultiLine ? fm.leading() + m_extraLeading : m_extraLeading;
    m_CutMessage.clear();
    m_textCursor = -1;

    if (m_Message != m_DefaultMessage)
    {
        bool isNumber;
        int value = m_Message.toInt(&isNumber);

        if (isNumber && m_TemplateText.contains("%n"))
        {
            m_CutMessage = qApp->translate("ThemeUI",
                                           m_TemplateText.toUtf8(), NULL,
                                           QCoreApplication::UnicodeUTF8,
                                           qAbs(value));
        }
        else if (m_TemplateText.contains("%1"))
        {
            QString tmp = qApp->translate("ThemeUI", m_TemplateText.toUtf8(),
                                          NULL, QCoreApplication::UnicodeUTF8);
            m_CutMessage = tmp.arg(m_Message);
        }
    }

    if (m_CutMessage.isEmpty())
        m_CutMessage = m_Message;

    if (m_CutMessage.isEmpty())
    {
        if (m_Layouts.empty())
            m_Layouts.push_back(new QTextLayout);

        QTextLine line;
        QTextOption textoption(static_cast<Qt::Alignment>(m_Justification));
        QVector<QTextLayout *>::iterator Ilayout = m_Layouts.begin();

        (*Ilayout)->setTextOption(textoption);
        (*Ilayout)->setText("");
        (*Ilayout)->beginLayout();
        line = (*Ilayout)->createLine();
        line.setLineWidth(m_Area.width());
        line.setPosition(QPointF(0, 0));
        (*Ilayout)->endLayout();
        m_drawRect.setWidth(m_Area.width());
        m_drawRect.setHeight(m_lineHeight);

        for (++Ilayout ; Ilayout != m_Layouts.end(); ++Ilayout)
            (*Ilayout)->clearLayout();

        m_Ascent = m_Descent = m_leftBearing = m_rightBearing = 0;
    }
    else
    {
        QStringList templist;
        QStringList::iterator it;

        switch (m_textCase)
        {
          case CaseUpper :
            m_CutMessage = m_CutMessage.toUpper();
            break;
          case CaseLower :
            m_CutMessage = m_CutMessage.toLower();
            break;
          case CaseCapitaliseFirst :
            //m_CutMessage = m_CutMessage.toLower();
            templist = m_CutMessage.split(". ");

            for (it = templist.begin(); it != templist.end(); ++it)
                (*it).replace(0, 1, (*it).left(1).toUpper());

            m_CutMessage = templist.join(". ");
            break;
          case CaseCapitaliseAll :
            //m_CutMessage = m_CutMessage.toLower();
            templist = m_CutMessage.split(" ");

            for (it = templist.begin(); it != templist.end(); ++it)
                (*it).replace(0, 1, (*it).left(1).toUpper());

            m_CutMessage = templist.join(" ");
            break;
            case CaseNormal:
                break;
        }

        QTextOption textoption(static_cast<Qt::Alignment>(m_Justification));
        textoption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

        int   idx, num_lines;
        qreal last_line_width;
        QStringList paragraphs = m_CutMessage.split('\n',
                                                    QString::KeepEmptyParts);

        for (idx = m_Layouts.size(); idx < paragraphs.size(); ++idx)
            m_Layouts.push_back(new QTextLayout);

        if (m_MultiLine && m_ShrinkNarrow &&
            m_MinSize.isValid() && !m_CutMessage.isEmpty())
            GetNarrowWidth(paragraphs, textoption, width);
        else
            width = m_Area.width();

        height = 0;
        m_leftBearing = m_rightBearing = 0;
        LayoutParagraphs(paragraphs, textoption, width, height,
                         min_rect, last_line_width, num_lines, true);

        m_Canvas.setRect(0, 0, min_rect.x() + min_rect.width(), height);
        m_scrollPause = m_scrollStartDelay; // ????
        m_scrollBounce = false;

        /**
         * FontMetrics::height() returns a value that is good for spacing
         * the lines, but may not represent the *full* height.  We need
         * to make sure we have enough space for the *full* height or
         * characters could be clipped.
         */
        QRect actual = fm.boundingRect(m_CutMessage);
        m_Ascent = -(actual.y() + fm.ascent());
        m_Descent = actual.height() - fm.height();
    }

    if (m_scrolling)
    {
        if (m_scrollDirection == ScrollLeft ||
            m_scrollDirection == ScrollRight ||
            m_scrollDirection == ScrollHorizontal)
        {
            if (m_Canvas.width() > m_drawRect.width())
            {
                m_drawRect.setX(m_Area.x());
                m_drawRect.setWidth(m_Area.width());
                m_scrollOffset = m_drawRect.x() - m_Canvas.x();
            }
        }
        else
        {
            if (m_Canvas.height() > m_drawRect.height())
            {
                m_drawRect.setY(m_Area.y());
                m_drawRect.setHeight(m_Area.height());
                m_scrollOffset = m_drawRect.y() - m_Canvas.y();
            }
        }
    }

    // If any of hcenter|vcenter|Justify, center it all, then adjust later
    if (m_Justification & (Qt::AlignCenter|Qt::AlignJustify))
    {
        m_drawRect.moveCenter(m_Area.center());
        min_rect.moveCenter(m_Area.center());
    }

    // Adjust horizontal
    if (m_Justification & Qt::AlignLeft)
    {
        // If text size is less than allowed min size, center it
        if (m_ShrinkNarrow && m_MinSize.isValid() && min_rect.isValid() &&
            min_rect.width() < m_MinSize.x())
        {
            m_drawRect.moveLeft(m_Area.x() +
                                (((m_MinSize.x() - min_rect.width() +
                                   fm.averageCharWidth()) / 2)));
            min_rect.setWidth(m_MinSize.x());
        }
        else
            m_drawRect.moveLeft(m_Area.x());

        min_rect.moveLeft(m_Area.x());
    }
    else if (m_Justification & Qt::AlignRight)
    {
        // If text size is less than allowed min size, center it
        if (m_ShrinkNarrow && m_MinSize.isValid() && min_rect.isValid() &&
            min_rect.width() < m_MinSize.x())
        {
            m_drawRect.moveRight(m_Area.x() + m_Area.width() -
                                (((m_MinSize.x() - min_rect.width() +
                                   fm.averageCharWidth()) / 2)));
            min_rect.setWidth(m_MinSize.x());
        }
        else
            m_drawRect.moveRight(m_Area.x() + m_Area.width());

        min_rect.moveRight(m_Area.x() + m_Area.width());
    }

    // Adjust vertical
    if (m_Justification & Qt::AlignTop)
    {
        // If text size is less than allowed min size, center it
        if (!m_ShrinkNarrow && m_MinSize.isValid() && min_rect.isValid() &&
            min_rect.height() < m_MinSize.y())
        {
            m_drawRect.moveTop(m_Area.y() +
                               ((m_MinSize.y() - min_rect.height()) / 2));
            min_rect.setHeight(m_MinSize.y());
        }
        else
            m_drawRect.moveTop(m_Area.y());

        min_rect.moveTop(m_Area.y());
    }
    else if (m_Justification & Qt::AlignBottom)
    {
        // If text size is less than allowed min size, center it
        if (!m_ShrinkNarrow && m_MinSize.isValid() && min_rect.isValid() &&
            min_rect.height() < m_MinSize.y())
        {
            m_drawRect.moveBottom(m_Area.y() + m_Area.height() -
                                  ((m_MinSize.y() - min_rect.height()) / 2));
            min_rect.setHeight(m_MinSize.y());
        }
        else
            m_drawRect.moveBottom(m_Area.y() + m_Area.height());

        min_rect.moveBottom(m_Area.y() + m_Area.height());
    }

    m_Initiator = m_EnableInitiator;
    if (m_MinSize.isValid())
    {
        // Record the minimal area needed for the message.
        SetMinArea(min_rect.toRect());
    }
}

QPoint MythUIText::CursorPosition(int text_offset)
{
    if (m_Layouts.empty())
        return m_Area.topLeft().toQPoint();

    if (text_offset == m_textCursor)
        return m_cursorPos;
    m_textCursor = text_offset;

    QVector<QTextLayout *>::const_iterator Ipara;
    QPoint pos;
    int    x, y, mid, line_height;
    int    offset = text_offset;

    for (Ipara = m_Layouts.constBegin(); Ipara != m_Layouts.constEnd(); ++Ipara)
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
    if (Ipara == m_Layouts.constEnd())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("'%1' (%2) CursorPosition offset %3 not found in "
                    "ANY paragraph!")
            .arg(objectName()).arg(GetXMLLocation()).arg(text_offset));
        return m_Area.topLeft().toQPoint();
    }

    mid = m_drawRect.width() / 2;
    if (m_Canvas.width() <= m_drawRect.width() || pos.x() <= mid)
        x = 0;  // start
    else if (pos.x() >= m_Canvas.width() - mid) // end
    {
        x = m_Canvas.width() - m_drawRect.width();
        pos.setX(pos.x() - x);
    }
    else // middle
    {
        x = pos.x() - mid;
        pos.setX(pos.x() - x);
    }

    line_height = m_lineHeight + m_Leading;
    mid = m_Area.height() / 2;
    mid -= (mid % line_height);
    y = pos.y() - mid;

    if (y <= 0 || m_Canvas.height() <= m_Area.height()) // Top of buffer
        y = 0;
    else if (y + m_Area.height() > m_Canvas.height()) // Bottom of buffer
    {
        int visible_lines = ((m_Area.height() / line_height) * line_height);
        y = m_Canvas.height() - visible_lines;
        pos.setY(visible_lines - (m_Canvas.height() - pos.y()));
    }
    else // somewhere in the middle
    {
        y -= m_Leading;
        pos.setY(mid + m_Leading);
    }

    m_Canvas.moveTopLeft(QPoint(-x, -y));

    // Compensate for vertical alignment
    pos.setY(pos.y() + m_drawRect.y() - m_Area.y());

    pos += m_Area.topLeft().toQPoint();
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

    if (m_colorCycling)
    {
        curR += incR;
        curG += incG;
        curB += incB;

        m_curStep++;

        if (m_curStep >= m_numSteps)
        {
            m_curStep = 0;
            incR *= -1;
            incG *= -1;
            incB *= -1;
        }

        QColor newColor = QColor((int)curR, (int)curG, (int)curB);

        if (newColor != m_Font->color())
        {
            m_Font->SetColor(newColor);
            SetRedraw();
        }
    }

    if (m_scrolling)
    {
        int whole;

        if (m_scrollPause > 0)
            --m_scrollPause;
        else
        {
            if (m_scrollBounce)
                m_scrollPos += m_scrollReturnRate;
            else
                m_scrollPos += m_scrollForwardRate;
        }

        whole = static_cast<int>(m_scrollPos);
        if (m_scrollPosWhole != whole)
        {
            int shift = whole - m_scrollPosWhole;
            m_scrollPosWhole = whole;

            switch (m_scrollDirection)
            {
              case ScrollLeft :
                if (m_Canvas.width() > m_drawRect.width())
                {
                    ShiftCanvas(-shift, 0);
                    if (m_Canvas.x() + m_Canvas.width() < 0)
                    {
                        SetCanvasPosition(m_drawRect.width(), 0);
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                }
                break;
              case ScrollRight :
                if (m_Canvas.width() > m_drawRect.width())
                {
                    ShiftCanvas(shift, 0);
                    if (m_Canvas.x() > m_drawRect.width())
                    {
                        SetCanvasPosition(-m_Canvas.width(), 0);
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                }
                break;
              case ScrollHorizontal:
                if (m_Canvas.width() <= m_drawRect.width())
                    break;
                if (m_scrollBounce) // scroll right
                {
                    if (m_Canvas.x() + m_scrollOffset > m_drawRect.x())
                    {
                        m_scrollBounce = false;
                        m_scrollPause = m_scrollStartDelay;
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                    else
                        ShiftCanvas(shift, 0);
                }
                else // scroll left
                {
                    if (m_Canvas.x() + m_Canvas.width() + m_scrollOffset <
                        m_drawRect.x() + m_drawRect.width())
                    {
                        m_scrollBounce = true;
                        m_scrollPause = m_scrollReturnDelay;
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                    else
                        ShiftCanvas(-shift, 0);
                }
                break;
              case ScrollUp :
                if (m_Canvas.height() > m_drawRect.height())
                {
                    ShiftCanvas(0, -shift);
                    if (m_Canvas.y() + m_Canvas.height() < 0)
                    {
                        SetCanvasPosition(0, m_drawRect.height());
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                }
                break;
              case ScrollDown :
                if (m_Canvas.height() > m_drawRect.height())
                {
                    ShiftCanvas(0, shift);
                    if (m_Canvas.y() > m_drawRect.height())
                    {
                        SetCanvasPosition(0, -m_Canvas.height());
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                }
                break;
              case ScrollVertical:
                if (m_Canvas.height() <= m_drawRect.height())
                    break;
                if (m_scrollBounce) // scroll down
                {
                    if (m_Canvas.y() + m_scrollOffset > m_drawRect.y())
                    {
                        m_scrollBounce = false;
                        m_scrollPause = m_scrollStartDelay;
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                    else
                        ShiftCanvas(0, shift);
                }
                else // scroll up
                {
                    if (m_Canvas.y() + m_Canvas.height() + m_scrollOffset <
                        m_drawRect.y() + m_drawRect.height())
                    {
                        m_scrollBounce = true;
                        m_scrollPause = m_scrollReturnDelay;
                        m_scrollPos = m_scrollPosWhole = 0;
                    }
                    else
                        ShiftCanvas(0, -shift);
                }
                break;
              case ScrollNone:
                break;
            }
        }
    }
}

void MythUIText::CycleColor(QColor startColor, QColor endColor, int numSteps)
{
    if (!GetPainter()->SupportsAnimation())
        return;

    m_startColor = startColor;
    m_endColor = endColor;
    m_numSteps = numSteps;
    m_curStep = 0;

    curR = startColor.red();
    curG = startColor.green();
    curB = startColor.blue();

    incR = (endColor.red() * 1.0 - curR) / m_numSteps;
    incG = (endColor.green() * 1.0 - curG) / m_numSteps;
    incB = (endColor.blue() * 1.0 - curB) / m_numSteps;

    m_colorCycling = true;
}

void MythUIText::StopCycling(void)
{
    if (!m_colorCycling)
        return;

    m_Font->SetColor(m_startColor);
    m_colorCycling = false;
    SetRedraw();
}

bool MythUIText::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
        m_OrigDisplayRect = m_Area;
    }
    //    else if (element.tagName() == "altarea") // Unused, but maybe in future?
    //        m_AltDisplayRect = parseRect(element);
    else if (element.tagName() == "font")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *fp = GetFont(fontname);

        if (!fp)
            fp = GetGlobalFontMap()->GetFont(fontname);

        if (fp)
        {
            MythFontProperties font = *fp;
            int screenHeight = GetMythMainWindow()->GetUIScreenRect().height();
            font.Rescale(screenHeight);
            int fontStretch = GetMythUI()->GetFontStretch();
            font.AdjustStretch(fontStretch);
            QString state = element.attribute("state", "");

            if (!state.isEmpty())
            {
                m_FontStates.insert(state, font);
            }
            else
            {
                m_FontStates.insert("default", font);
                *m_Font = m_FontStates["default"];
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
            m_Message = qApp->translate("ThemeUI",
                                        parseText(element).toUtf8(), NULL,
                                        QCoreApplication::UnicodeUTF8);
        }
        else if (element.attribute("lang", "").toLower() ==
                 gCoreContext->GetLanguageAndVariant())
        {
            m_Message = parseText(element);
        }
        else if (element.attribute("lang", "").toLower() ==
                 gCoreContext->GetLanguage())
        {
            m_Message = parseText(element);
        }

        m_DefaultMessage = m_Message;
        SetText(m_Message);
    }
    else if (element.tagName() == "template")
    {
        m_TemplateText = parseText(element);
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
            m_colorCycling = false;

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
                        .arg(objectName()).arg(GetXMLLocation()));
                }
            }

            tmp = element.attribute("startdelay");
            if (!tmp.isEmpty())
            {
                float seconds = tmp.toFloat();
                m_scrollStartDelay = static_cast<int>(seconds *
                      static_cast<float>(MythMainWindow::drawRefresh) + 0.5);
            }
            tmp = element.attribute("returndelay");
            if (!tmp.isEmpty())
            {
                float seconds = tmp.toFloat();
                m_scrollReturnDelay = static_cast<int>(seconds *
                      static_cast<float>(MythMainWindow::drawRefresh) + 0.5);
            }
            tmp = element.attribute("rate");
            if (!tmp.isEmpty())
            {
#if 0 // scroll rate as a percentage of 70Hz
                float percent = tmp.toFloat() / 100.0;
                m_scrollForwardRate = percent *
                      static_cast<float>(MythMainWindow::drawRefresh) / 70.0;
#else // scroll rate as pixels per second
                int pixels = tmp.toInt();
                m_scrollForwardRate =
                    pixels / static_cast<float>(MythMainWindow::drawRefresh);
#endif
            }
            tmp = element.attribute("returnrate");
            if (!tmp.isEmpty())
            {
#if 0 // scroll rate as a percentage of 70Hz
                float percent = tmp.toFloat() / 100.0;
                m_scrollReturnRate = percent *
                      static_cast<float>(MythMainWindow::drawRefresh) / 70.0;
#else // scroll rate as pixels per second
                int pixels = tmp.toInt();
                m_scrollReturnRate =
                    pixels / static_cast<float>(MythMainWindow::drawRefresh);
#endif
            }

            m_scrolling = true;
        }
        else
            m_scrolling = false;
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
            m_ShrinkNarrow = (element.attribute("shrink")
                              .toLower() != "short");
        }

        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

void MythUIText::CopyFrom(MythUIType *base)
{
    MythUIText *text = dynamic_cast<MythUIText *>(base);

    if (!text)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("'%1' (%2) ERROR, bad parsing '%3' (%4)")
            .arg(objectName()).arg(GetXMLLocation())
            .arg(base->objectName()).arg(base->GetXMLLocation()));
        return;
    }

    m_Justification = text->m_Justification;
    m_OrigDisplayRect = text->m_OrigDisplayRect;
    m_AltDisplayRect = text->m_AltDisplayRect;
    m_Canvas = text->m_Canvas;
    m_drawRect = text->m_drawRect;

    m_DefaultMessage = text->m_DefaultMessage;
    SetText(text->m_Message);
    m_CutMessage = text->m_CutMessage;
    m_TemplateText = text->m_TemplateText;

    m_ShrinkNarrow = text->m_ShrinkNarrow;
    m_Cutdown = text->m_Cutdown;
    m_MultiLine = text->m_MultiLine;
    m_Leading = text->m_Leading;
    m_extraLeading = text->m_extraLeading;
    m_lineHeight = text->m_lineHeight;
    m_textCursor = text->m_textCursor;

    QMutableMapIterator<QString, MythFontProperties> it(text->m_FontStates);

    while (it.hasNext())
    {
        it.next();
        m_FontStates.insert(it.key(), it.value());
    }

    *m_Font = m_FontStates["default"];

    m_colorCycling = text->m_colorCycling;
    m_startColor = text->m_startColor;
    m_endColor = text->m_endColor;
    m_numSteps = text->m_numSteps;
    m_curStep = text->m_curStep;
    curR = text->curR;
    curG = text->curG;
    curB = text->curB;
    incR = text->incR;
    incG = text->incG;
    incB = text->incB;

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
    MythUIText *text = new MythUIText(parent, objectName());
    text->CopyFrom(this);
}


void MythUIText::Finalize(void)
{
    if (m_scrolling && m_Cutdown != Qt::ElideNone)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("'%1' (%2): <scroll> and <cutdown> are not combinable.")
            .arg(objectName()).arg(GetXMLLocation()));
        m_Cutdown = Qt::ElideNone;
    }
    FillCutMessage();
}
