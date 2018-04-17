
#include "mythuiguidegrid.h"

// ANSI C headers
#include <cmath>

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <QFile>
#include <QDomElement>

// myth
#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "x11colors.h"
#include "mythlogging.h"
#include "mythimage.h"
#include "mythuitype.h"
#include "mythuiimage.h"
#include "mythmainwindow.h"
#include "mythdb.h"

#define LOC QString("MythUIGuideGrid: ")

MythUIGuideGrid::MythUIGuideGrid(MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
    m_allData(NULL)
{
    // themeable setting defaults
    for (uint x = 0; x < RECSTATUSSIZE; x++)
        m_recImages[x] = NULL;

    for (uint x = 0; x < ARROWIMAGESIZE; x++)
        m_arrowImages[x] = NULL;

    m_channelCount = 5;
    m_timeCount = 4;
    m_verticalLayout = false;

    m_font = new MythFontProperties();
    m_justification = Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap;
    m_multilineText = true;
    m_cutdown = true;

    m_selType = "box";
    m_drawSelLine = QPen(Qt::NoPen);
    m_drawSelFill = QBrush(Qt::NoBrush);

    m_fillType = Solid;

    m_rowCount = 0;
    m_progPastCol = 0;

    m_drawCategoryColors = true;
    m_drawCategoryText = true;
    m_categoryAlpha = 255;

    QMap<QString, QString> catColors;
    parseDefaultCategoryColors(catColors);
    SetCategoryColors(catColors);

}

void MythUIGuideGrid::Finalize(void)
{
    m_rowCount = m_channelCount;

    m_allData = new QList<UIGTCon *>[m_rowCount];

    MythUIType::Finalize();
}

MythUIGuideGrid::~MythUIGuideGrid()
{
    for (int i = 0; i < m_rowCount; i++)
        ResetRow(i);

    delete [] m_allData;

    delete m_font;
    m_font = NULL;

    // The m_recImages and m_arrowImages images are now children of
    // the MythGuiGuideGrid widget and should be automatically deleted
    // when it is deleted.
}

bool MythUIGuideGrid::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "layout")
    {
        QString layout = getFirstText(element).toLower();
        m_verticalLayout = (layout == "vertical");
    }
    else if (element.tagName() == "channels")
    {
        m_channelCount = getFirstText(element).toInt();
        m_channelCount = max(m_channelCount, 1);
        m_channelCount = min(m_channelCount, MAX_DISPLAY_CHANS);
    }
    else if (element.tagName() == "timeslots")
    {
        m_timeCount = getFirstText(element).toInt();
        m_timeCount = max(m_timeCount, 1);
        m_timeCount = min(m_timeCount, 8);
    }
    else if (element.tagName() == "solidcolor")
    {
        QString color = getFirstText(element);
        m_solidColor = QColor(color);
    }
    else if (element.tagName() == "selector")
    {
        m_selType = element.attribute("type");
        QString lineColor = element.attribute("linecolor", "");
        QString fillColor = element.attribute("fillcolor", "");

        if (!lineColor.isEmpty())
        {
            m_drawSelLine = QPen(QColor(lineColor));
            m_drawSelLine.setWidth(2);
        }
        else
        {
            m_drawSelLine = QPen(Qt::NoPen);
        }

        if (!fillColor.isEmpty())
        {
            m_drawSelFill = QBrush(QColor(fillColor));
        }
        else
        {
            m_drawSelFill = QBrush(Qt::NoBrush);
        }
    }
    else if (element.tagName() == "recordingcolor")
    {
        QString color = getFirstText(element);
        m_recordingColor = QColor(color);
    }
    else if (element.tagName() == "conflictingcolor")
    {
        QString color = getFirstText(element);
        m_conflictingColor = QColor(color);
    }
    else if (element.tagName() == "categoryalpha")
    {
        m_categoryAlpha = getFirstText(element).toInt();
        m_categoryAlpha = max(m_categoryAlpha, 1);
        m_categoryAlpha = min(m_categoryAlpha, 255);
    }
    else if (element.tagName() == "showcategories")
    {
        m_drawCategoryText = parseBool(element);
    }
    else if (element.tagName() == "showcategorycolors")
    {
        m_drawCategoryColors = parseBool(element);
    }
    else if (element.tagName() == "cutdown")
    {
        m_cutdown = parseBool(element);
    }
    else if (element.tagName() == "multiline")
    {
        SetMultiLine(parseBool(element));
    }
    else if (element.tagName() == "textoffset")
    {
        m_textOffset =  parsePoint(element);
    }
    else if (element.tagName() == "font")
    {
        QString fontname = getFirstText(element);
        MythFontProperties *font = GetFont(fontname);

        if (!font)
            font = GetGlobalFontMap()->GetFont(fontname);

        if (font)
        {
            MythFontProperties fontcopy = *font;
            int screenHeight = GetMythMainWindow()->GetUIScreenRect().height();
            fontcopy.Rescale(screenHeight);
            int fontStretch = GetMythUI()->GetFontStretch();
            fontcopy.AdjustStretch(fontStretch);
            *m_font = fontcopy;
        }
        else
            LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown font: " + fontname);
    }
    else if (element.tagName() == "recordstatus")
    {
        int inttype = 0;
        QString typ = element.attribute("type");
        QString img = element.attribute("image");

        if (typ == "SingleRecord")
            inttype = 1;
        else if (typ == "TimeslotRecord")
            inttype = 2;
        else if (typ == "ChannelRecord")
            inttype = 3;
        else if (typ == "AllRecord")
            inttype = 4;
        else if (typ == "WeekslotRecord")
            inttype = 5;
        else if (typ == "FindOneRecord")
            inttype = 6;
        else if (typ == "OverrideRecord")
            inttype = 7;

        LoadImage(inttype, img);
    }
    else if (element.tagName() == "arrow")
    {
        QString dir = element.attribute("direction");
        QString image = element.attribute("image");

        if (dir == "left")
            SetArrow(0, image);
        else if (dir == "right")
            SetArrow(1, image);
        else if (dir == "up")
            SetArrow(2, image);
        else if (dir == "down")
            SetArrow(3, image);
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

void MythUIGuideGrid::CopyFrom(MythUIType *base)
{
    MythUIGuideGrid *gg = dynamic_cast<MythUIGuideGrid *>(base);

    if (!gg)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "bad parsing");
        return;
    }

    m_channelCount = gg->m_channelCount;
    m_timeCount = gg->m_timeCount;
    m_verticalLayout = gg->m_verticalLayout;
    m_categoryAlpha = gg->m_categoryAlpha;
    m_textOffset = gg->m_textOffset;
    m_justification = gg->m_justification;
    m_multilineText = gg->m_multilineText;
    *m_font = *gg->m_font;
    m_solidColor = gg->m_solidColor;

    m_selType = gg->m_selType;
    m_drawSelLine = gg->m_drawSelLine;
    m_drawSelFill = gg->m_drawSelFill;

    m_recordingColor = gg->m_recordingColor;
    m_conflictingColor = gg->m_conflictingColor;

    m_fillType = gg->m_fillType;
    m_cutdown = gg->m_cutdown;
    m_drawCategoryColors = gg->m_drawCategoryColors;
    m_drawCategoryText = gg->m_drawCategoryText;

    MythUIType::CopyFrom(base);
}

void MythUIGuideGrid::CreateCopy(MythUIType *parent)
{
    MythUIGuideGrid *gg = new MythUIGuideGrid(parent, objectName());
    gg->CopyFrom(this);
}

QColor MythUIGuideGrid::calcColor(const QColor &color, int alphaMod)
{
    QColor newColor(color);
    newColor.setAlpha((int)(color.alpha() *(alphaMod / 255.0)));
    return newColor;
}

/** \fn MythUIGuideGrid::DrawSelf(MythPainter *, int, int, int, QRect)
 *  \brief Draws an entire GuideGrid.
 *
 *  Draw the complete contents of a GuideGrid. This function iterates
 *  over all the rows and columns in a guide grid, and calls the
 *  appropriate functions to draw the grid items. This is accomplished
 *  in three stages. First all the item backgrounds are drawn in
 *  appropriate colors for their genre and recoding state, then the
 *  background/decorations for the selected item are drawn, and
 *  finally all the item texts and recording decorators are drawn on
 *  top.
 *
 *  @note This function does not translate local->global offsets. All
 *  of the drawing functions it calls must perform this translation.
 *
 *  \param p           A pointer to the MythPainter structure that
 *                     will be used to render this object onto the screen.
 *  \param xoffset     The X offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param yoffset     The Y offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param alphaMod    The alpha (transparency) value for this widget.
 *  \param clipRect    Ignored.
 */
void MythUIGuideGrid::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                               int alphaMod, QRect clipRect)
{
    p->SetClipRect(clipRect);
    for (int i = 0; i < m_rowCount; i++)
    {
        QList<UIGTCon *>::iterator it = m_allData[i].begin();

        for (; it != m_allData[i].end(); ++it)
        {
            UIGTCon *data = *it;

            if (data->m_recStat == 0)
                drawBackground(p, xoffset, yoffset, data, alphaMod);
            else if (data->m_recStat == 1)
                drawBox(p, xoffset, yoffset, data, m_recordingColor, alphaMod);
            else
                drawBox(p, xoffset, yoffset, data, m_conflictingColor, alphaMod);
        }
    }

    drawCurrent(p, xoffset, yoffset, &m_selectedItem, alphaMod);

    for (int i = 0; i < m_rowCount; i++)
    {
        QList<UIGTCon *>::iterator it = m_allData[i].begin();

        for (; it != m_allData[i].end(); ++it)
        {
            UIGTCon *data = *it;
            drawText(p, xoffset, yoffset, data, alphaMod);

            if (data->m_recType != 0 || data->m_arrow != GridTimeNormal)
                drawRecDecoration(p, xoffset, yoffset, data, alphaMod);
        }
    }
}

/** \fn MythUIGuideGrid::drawCurrent(MythPainter *, int, int, UIGTCon *, int)
 *  \brief Draws selection indication for a GuideGrid item.
 *
 *  This function is responsible for drawing decoration items that are
 *  unique to the currently selected entry. This may be a highlight
 *  rectangle around the entry, or special colors to indicate the
 *  currently selected entry. The type of decoration drawn depends
 *  upon the setting on the local m_SelType variable.
 *
 *  \param p           A pointer to the MythPainter structure that
 *                     will be used to render this object onto the screen.
 *  \param xoffset     The X offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param yoffset     The Y offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param data        A pointer to the GuideGrid object to be drawn.
 *  \param alphaMod    The alpha (transparency) value for this widget.
 */
void MythUIGuideGrid::drawCurrent(MythPainter *p, int xoffset, int yoffset, UIGTCon *data, int alphaMod)
{
    int breakin = 2;
    QRect area = data->m_drawArea;
    area.translate(m_Area.x(), m_Area.y());	// Adjust within parent
    area.translate(xoffset, yoffset);		// Convert to global coordinates
    area.adjust(breakin, breakin, -breakin, -breakin);
    int status = data->m_recStat;

    if (m_selType == "roundbox")
    {
        QPen pen = m_drawSelLine;

        if (status == 1)
            pen.setColor(m_recordingColor);
        else if (status == 2)
            pen.setColor(m_conflictingColor);

        p->DrawRoundRect(area, 10, m_drawSelFill, pen, alphaMod);
    }
    else if (m_selType == "highlight")
    {
        QBrush brush = m_drawSelFill;
        QPen   pen   = m_drawSelLine;

        if (m_drawCategoryColors && data->m_categoryColor.isValid())
            brush.setColor(calcColor(data->m_categoryColor, m_categoryAlpha));
        else
            brush.setColor(calcColor(m_solidColor, m_categoryAlpha));

        if (status == 1)
            pen.setColor(m_recordingColor);
        else if (status == 2)
            pen.setColor(m_conflictingColor);

        brush.setColor(brush.color().lighter());
        p->DrawRect(area, brush, pen, alphaMod);
    }
    else
    {
        // default to "box" selection type
        QPen pen = m_drawSelLine;

        if (status == 1)
            pen.setColor(m_recordingColor);
        else if (status == 2)
            pen.setColor(m_conflictingColor);

        p->DrawRect(area, m_drawSelFill, pen, alphaMod);
    }
}

/** \fn MythUIGuideGrid::drawRecDecoration(MythPainter *, int, int, UIGTCon *, int)
 *  \brief Draws decoration items for a GuideGrid item.
 *
 *  This function is responsible for drawing the decoration items onto
 *  an entry in the GuideGrid. It draws left/right (or up/down) arrows
 *  if the program time extends before or after what's visible in the
 *  window. If a show is scheduled to record, it also draws the badges
 *  that indicates the type of recording.
 *
 *  \param p           A pointer to the MythPainter structure that
 *                     will be used to render this object onto the screen.
 *  \param xoffset     The X offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param yoffset     The Y offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param data        A pointer to the GuideGrid object to be drawn.
 *  \param alphaMod    The alpha (transparency) value for this widget.
 */
void MythUIGuideGrid::drawRecDecoration(MythPainter *p, int xoffset, int yoffset, UIGTCon *data, int alphaMod)
{
    int breakin = 1;
    QRect area = data->m_drawArea;
    area.translate(m_Area.x(), m_Area.y());	// Adjust within parent
    area.translate(xoffset, yoffset);		// Convert to global coordinates
    area.adjust(breakin, breakin, -breakin, -breakin);

    // draw arrows
    if (data->m_arrow != GridTimeNormal)
    {
        if (data->m_arrow & GridTimeStartsBefore)
        {
            if (m_verticalLayout)
            {
                if (m_arrowImages[2]) { // UP
                    MythUIImage *arrow = m_arrowImages[2];
                    MythRect arrowarea = arrow->GetArea();
                    arrow->DrawSelf(p, area.center().x() - (arrowarea.width() / 2),
                                    area.top(), alphaMod, area);
                }
            }
            else
            {
                if (m_arrowImages[0]) { // LEFT
                    MythUIImage *arrow = m_arrowImages[0];
                    MythRect arrowarea = arrow->GetArea();
                    arrow->DrawSelf(p, area.left(),
                                    area.center().y() - (arrowarea.height() / 2),
                                    alphaMod, area);
                }
            }
        }

        if (data->m_arrow & GridTimeEndsAfter)
        {
            if (m_verticalLayout)
            {
                if (m_arrowImages[3]) { // BOTTOM
                    MythUIImage *arrow = m_arrowImages[3];
                    MythRect arrowarea = arrow->GetArea();
                    arrow->DrawSelf(p, area.center().x() - (arrowarea.width() / 2),
                                    area.top() + area.height() - arrowarea.height(),
                                    alphaMod, area);
                }
            }
            else
            {
                if (m_arrowImages[1]) { // RIGHT
                    MythUIImage *arrow = m_arrowImages[1];
                    MythRect arrowarea = arrow->GetArea();
                    arrow->DrawSelf(p,
                                    area.right() - arrowarea.width(),
                                    area.center().y() - (arrowarea.height() / 2),
                                    alphaMod, area);
                }
            }
        }
    }

    // draw recording status
    if (data->m_recType != 0 && m_recImages[data->m_recType])
    {
        MythUIImage *image = m_recImages[data->m_recType];
        MythRect imagearea = image->GetArea();
        image->DrawSelf(p, area.right() - imagearea.width(),
                        area.bottom() - imagearea.height(),
                        alphaMod, area);
    }
}

/** \fn MythUIGuideGrid::drawBox(MythPainter *, int, int, UIGTCon *, int)
 *  \brief Draws the background for a GuideGrid item to be recorded.
 *
 *  This function is responsible for drawing the background behind all
 *  GuideGrid entries that are marked as being part of a recording
 *  rule. It draws a simple filled in box with no past/future
 *  demarcation.
 *
 *  \param p           A pointer to the MythPainter structure that
 *                     will be used to render this object onto the screen.
 *  \param xoffset     The X offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param yoffset     The Y offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param data        A pointer to the GuideGrid object to be drawn.
 *  \param color       The color to draw the box.
 *  \param alphaMod    The alpha (transparency) value for this widget.
 */
void MythUIGuideGrid::drawBox(MythPainter *p, int xoffset, int yoffset, UIGTCon *data, const QColor &color, int alphaMod)
{
    int breakin = 1;
    QRect area = data->m_drawArea;
    area.translate(m_Area.x(), m_Area.y());	// Adjust within parent
    area.translate(xoffset, yoffset);		// Convert to global coordinates
    area.adjust(breakin, breakin, -breakin, -breakin);

    static const QPen nopen(Qt::NoPen);
    p->DrawRect(area, QBrush(calcColor(color, m_categoryAlpha)), nopen, alphaMod);
}

/** \fn MythUIGuideGrid::drawBackground(MythPainter *, int, int, UIGTCon *, int)
 *  \brief Draws the background for a GuideGrid item that will not be recorded.
 *
 *  This function is responsible for drawing the background behind all
 *  GuideGrid entries that are not marked as being part of a recording
 *  rule. It is responsible for drawing the demarcation line in the
 *  grid between past and future times. It also chooses the background
 *  color appropriate to the genre of the show.
 *
 *  \param p           A pointer to the MythPainter structure that
 *                     will be used to render this object onto the screen.
 *  \param xoffset     The X offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param yoffset     The Y offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param data        A pointer to the GuideGrid object to be drawn.
 *  \param alphaMod    The alpha (transparency) value for this widget.
 */
void MythUIGuideGrid::drawBackground(MythPainter *p, int xoffset, int yoffset, UIGTCon *data, int alphaMod)
{
    QColor overColor;
    QRect overArea;

    int breakin = 1;
    QRect area = data->m_drawArea;
    area.translate(m_Area.x(), m_Area.y());	// Adjust within parent
    QColor fillColor;

    if (m_drawCategoryColors && data->m_categoryColor.isValid())
        fillColor = calcColor(data->m_categoryColor, m_categoryAlpha);
    else
        fillColor = calcColor(m_solidColor, m_categoryAlpha);

    // These calculations are in the parents local coordinates
    if (m_verticalLayout)
    {
        if (m_progPastCol && area.top() < m_progPastCol)
        {
            if (area.bottom() < m_progPastCol)
            {
                fillColor = fillColor.dark();
                area.adjust(breakin, breakin, -breakin, -breakin);
            }
            else
            {
                overColor = fillColor.dark();
                int first = m_progPastCol - area.top();
                int second = area.height() - first;
                overArea = area;
                overArea.setHeight(first);
                area.translate(0, first);
                area.setHeight(second);

                area.adjust(0, -breakin, -breakin, -breakin);
                overArea.adjust(0, breakin, -breakin, -breakin);
            }
        }
        else
            area.adjust(breakin, breakin, -breakin, -breakin);
    }
    else
    {
        if (m_progPastCol && area.left() < m_progPastCol)
        {
            if (area.right() < m_progPastCol)
            {
                fillColor = fillColor.dark();
                area.adjust(breakin, breakin, -breakin, -breakin);
            }
            else
            {
                overColor = fillColor.dark();
                int first = m_progPastCol - area.left();
                int second = area.width() - first;
                overArea = area;
                overArea.setWidth(first);
                area.translate(first, 0);
                area.setWidth(second);

                area.adjust(0, breakin, -breakin, -breakin);
                overArea.adjust(breakin, breakin, 0, -breakin);
            }
        }
        else
            area.adjust(breakin, breakin, -breakin, -breakin);
    }

    if (area.width() <= 1)
        area.setWidth(2);

    if (area.height() <= 1)
        area.setHeight(2);

    static const QPen nopen(Qt::NoPen);
    area.translate(xoffset, yoffset);		// Convert to global coordinates
    p->DrawRect(area, QBrush(fillColor), nopen, alphaMod);

    if (overArea.width() > 0) {
        overArea.translate(xoffset, yoffset);	// Convert to global coordinates
        p->DrawRect(overArea, QBrush(overColor), nopen, alphaMod);
    }
}

/** \fn MythUIGuideGrid::drawText(MythPainter *, int, int, UIGTCon *, int)
 *  \brief Draws text strings for a GuideGrid item.
 *
 *  This function is responsible for drawing the text onto an entry in
 *  the GuideGrid. It is smart enough to leave space for the guide
 *  item continuation arrow to be drawn.
 *
 *  \param p           A pointer to the MythPainter structure that
 *                     will be used to render this object onto the screen.
 *  \param xoffset     The X offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param yoffset     The Y offset of the parent. Used to convert local
 *                     coordinates to global coordinates.
 *  \param data        A pointer to the GuideGrid object to be drawn.
 *  \param alphaMod    The alpha (transparency) value for this widget.
 */
void MythUIGuideGrid::drawText(MythPainter *p, int xoffset, int yoffset, UIGTCon *data, int alphaMod)
{
    QString msg = data->m_title;

    if (m_drawCategoryText && !data->m_category.isEmpty())
        msg += QString(" (%1)").arg(data->m_category);

    QRect area = data->m_drawArea;
    area.translate(m_Area.x(), m_Area.y());	// Adjust within parent
    area.translate(xoffset, yoffset);		// Convert to global coordinates
    area.adjust(m_textOffset.x(), m_textOffset.y(),
                -m_textOffset.x(), -m_textOffset.y());

    if (m_verticalLayout)
    {
        if ((data->m_arrow & GridTimeStartsBefore) && m_arrowImages[2])
            area.setTop(area.top() + m_arrowImages[2]->GetArea().height());

        if ((data->m_arrow  & GridTimeEndsAfter) && m_arrowImages[3])
            area.setBottom(area.bottom() - m_arrowImages[3]->GetArea().height());
    }
    else
    {
        if ((data->m_arrow  & GridTimeStartsBefore) && m_arrowImages[0])
            area.setLeft(area.left() + m_arrowImages[0]->GetArea().width());

        if ((data->m_arrow & GridTimeEndsAfter) && m_arrowImages[1])
            area.setRight(area.right() - m_arrowImages[1]->GetArea().width());
    }

    if (area.width() <= 0 || area.height() <= 0)
        return;

    p->DrawText(area, msg, m_justification, *m_font, alphaMod, area);
}

QPoint MythUIGuideGrid::GetRowAndColumn(QPoint position)
{
    for (int i = 0; i < m_rowCount; i++)
    {
        QList<UIGTCon *>::iterator it = m_allData[i].begin();

        for (int col = 0; it != m_allData[i].end(); ++it, ++col)
        {
            UIGTCon *data = *it;

            if (data->m_drawArea.contains(position))
            {
                return QPoint(col, i);
            }
        }
    }
    return QPoint(-1,-1);
}

void MythUIGuideGrid::SetProgramInfo(int row, int col, const QRect &area,
                                     const QString &title, const QString &genre,
                                     int arrow, int recType, int recStat,
                                     bool selected)
{
    (void)col;
    UIGTCon *data = new UIGTCon(area, title, genre, arrow, recType, recStat);
    m_allData[row].append(data);

    if (m_drawCategoryColors)
    {
        data->m_categoryColor = m_categoryColors[data->m_category.toLower()];

        if (!data->m_categoryColor.isValid())
            data->m_categoryColor = m_categoryColors["none"];
    }

    if (selected)
        m_selectedItem = *data;
}

bool MythUIGuideGrid::parseDefaultCategoryColors(QMap<QString, QString> &catColors)
{
    QFile f;
    QStringList searchpath = GetMythUI()->GetThemeSearchPath();

    for (QStringList::const_iterator ii = searchpath.begin();
         ii != searchpath.end(); ++ii)
    {
        f.setFileName(*ii + "categories.xml");

        if (f.open(QIODevice::ReadOnly))
            break;
    }

    if (f.handle() == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to open '%1'")
            .arg(f.fileName()));
        return false;
    }

    QDomDocument doc;
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Parsing colors: %1 at line: %2 column: %3")
            .arg(f.fileName()).arg(errorLine).arg(errorColumn) +
            QString("\n\t\t\t%1").arg(errorMsg));
        f.close();
        return false;
    }

    f.close();

    QDomElement element = doc.documentElement();

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();

        if (!info.isNull() && info.tagName() == "catcolor")
        {
            QString cat = info.attribute("category");
            QString col = info.attribute("color");

            catColors[cat.toLower()] = col;
        }
    }

    return true;
}

void MythUIGuideGrid::SetCategoryColors(const QMap<QString, QString> &catC)
{
    for (QMap<QString, QString>::const_iterator it = catC.begin();
         it != catC.end(); ++it)
    {
        m_categoryColors[it.key()] = createColor(*it);
    }
}

void MythUIGuideGrid::LoadImage(int recType, const QString &file)
{
    MythUIImage *uiimage = new MythUIImage(file, this, "guidegrid image");
    uiimage->m_imageProperties.isThemeImage = true;
    uiimage->SetVisible(false);
    uiimage->Load(false);

    MythUIImage *tmp = m_recImages[recType];
    m_recImages[recType] = uiimage;
    if (tmp)
        delete tmp;
}

void MythUIGuideGrid::SetArrow(int direction, const QString &file)
{
    MythUIImage *uiimage = new MythUIImage(file, this, "guidegrid arrow");
    uiimage->m_imageProperties.isThemeImage = true;
    uiimage->SetVisible(false);
    uiimage->Load(false);

    MythUIImage *tmp = m_arrowImages[direction];
    m_arrowImages[direction] = uiimage;
    if (tmp)
        delete tmp;
}

void MythUIGuideGrid::ResetData(void)
{
    for (int i = 0; i < m_rowCount; i++)
        ResetRow(i);
}

void MythUIGuideGrid::ResetRow(int row)
{
    while (!m_allData[row].empty())
    {
        delete m_allData[row].back();
        m_allData[row].pop_back();
    }
}

void MythUIGuideGrid::SetProgPast(int ppast)
{
    if (m_verticalLayout)
        m_progPastCol = m_Area.y() + (m_Area.height() * ppast / 100);
    else
        m_progPastCol = m_Area.x() + (m_Area.width() * ppast / 100);

    SetRedraw();
}

void MythUIGuideGrid::SetMultiLine(bool multiline)
{
    m_multilineText = multiline;

    if (m_multilineText)
        m_justification |= Qt::TextWordWrap;
    else
        m_justification &= ~Qt::TextWordWrap;
}
