#ifndef MYTHUIGUIDEGRID_H_
#define MYTHUIGUIDEGRID_H_

// QT
#include <QPixmap>
#include <QPen>
#include <QBrush>

// MythDB
#include "mythuiexp.h"

// MythUI
#include "mythuitype.h"

#define ARROWIMAGESIZE 4
#define RECSTATUSSIZE  8
#define MAX_DISPLAY_CHANS 40

class MythFontProperties;

/** \class MythUIGuideGrid
 *
 * \brief A narrow purpose widget used to show television programs and the
 *        timeslots they occupy on channels. Used for scheduling of recordings.
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIGuideGrid : public MythUIType
{
  public:
    MythUIGuideGrid(MythUIType *parent, const QString &name);
    ~MythUIGuideGrid();

    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    enum FillType { Alpha = 10, Dense, Eco, Solid };

    bool isVerticalLayout(void) { return m_verticalLayout; }
    int  getChannelCount(void) { return m_channelCount; }
    int  getTimeCount(void) { return m_timeCount; }

    void SetCategoryColors(const QMap<QString, QString> &catColors);

    void SetTextOffset(const QPoint &to) { m_textOffset = to; }
    void SetArrow(int, const QString &file);
    void LoadImage(int, const QString &file);
    void SetProgramInfo(int row, int col, const QRect &area,
                        const QString &title, const QString &category,
                        int arrow, int recType, int recStat, bool selected);
    void ResetData();
    void ResetRow(int row);
    void SetProgPast(int ppast);
    void SetMultiLine(bool multiline);

  protected:
    virtual void Finalize(void);
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    bool parseDefaultCategoryColors(QMap<QString, QString> &catColors);

  private:

    class UIGTCon
    {
      public:
        UIGTCon() { m_arrow = m_recType = m_recStat = 0; };
        UIGTCon(const QRect &drawArea, const QString &title,
                const QString &category, int arrow, int recType, int recStat) :
                m_drawArea(drawArea),               m_title(title),
                m_category(category.trimmed()),     m_arrow(arrow),
                m_recType(recType),                 m_recStat(recStat)
        {}

        UIGTCon(const UIGTCon &o) :
                m_drawArea(o.m_drawArea),   m_title(o.m_title),
                m_category(o.m_category),   m_categoryColor(o.m_categoryColor),
                m_arrow(o.m_arrow),         m_recType(o.m_recType),
                m_recStat(o.m_recStat)
        {}

        QRect m_drawArea;
        QString m_title;
        QString m_category;
        QColor m_categoryColor;
        int m_arrow;
        int m_recType;
        int m_recStat;
    };

    void drawBackground(MythPainter *p, UIGTCon *data, int alpaMod);
    void drawBox(MythPainter *p, UIGTCon *data, const QColor &color, int alpaMod);
    void drawText(MythPainter *p, UIGTCon *data, int alpaMod);
    void drawRecType(MythPainter *p, UIGTCon *data, int alpaMod);
    void drawCurrent(MythPainter *p, UIGTCon *data, int alpaMod);

    QColor calcColor(const QColor &color, int alpha);

    QList<UIGTCon*> *m_allData;
    UIGTCon m_selectedItem;

    MythImage *m_recImages[RECSTATUSSIZE];
    MythImage *m_arrowImages[ARROWIMAGESIZE];

    // themeable settings
    int  m_channelCount;
    int  m_timeCount;
    bool m_verticalLayout;
    QPoint m_textOffset;

    MythFontProperties *m_font;
    int    m_justification;
    bool   m_multilineText;
    bool   m_cutdown;

    QString m_selType;
    QPen    m_drawSelLine;
    QBrush  m_drawSelFill;

    QColor m_solidColor;
    QColor m_recordingColor;
    QColor m_conflictingColor;

    int    m_fillType;

    int  m_rowCount;
    int  m_progPastCol;

    bool   m_drawCategoryColors;
    bool   m_drawCategoryText;
    int    m_categoryAlpha;

    QMap<QString, QColor> m_categoryColors;

};

#endif
