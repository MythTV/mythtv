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
#include "mythuiimage.h"

#define ARROWIMAGESIZE 4
#define RECSTATUSSIZE  8


// max number of channels to display in the guide grid
#define MAX_DISPLAY_CHANS 40

// max number of 5 minute time slots to show in guide grid (48 = 4hrs)
#define MAX_DISPLAY_TIMES 48

#define GridTimeNormal       0
#define GridTimeStartsBefore 1
#define GridTimeEndsAfter    2

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

    void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                  int alphaMod, QRect clipRect) override; // MythUIType

    enum FillType { Alpha = 10, Dense, Eco, Solid };

    bool isVerticalLayout(void) { return m_verticalLayout; }
    int  getChannelCount(void) { return m_channelCount; }
    int  getTimeCount(void) { return m_timeCount; }

    void SetCategoryColors(const QMap<QString, QString> &catColors);

    void SetTextOffset(const QPoint &to) { m_textOffset = to; }
    void SetArrow(int, const QString &file);
    void LoadImage(int, const QString &file);
    void SetProgramInfo(int row, int col, const QRect &area,
                        const QString &title, const QString &genre,
                        int arrow, int recType, int recStat, bool selected);
    void ResetData();
    void ResetRow(int row);
    void SetProgPast(int ppast);
    void SetMultiLine(bool multiline);

    QPoint GetRowAndColumn(QPoint position);

  protected:
    void Finalize(void) override; // MythUIType
    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType

    static bool parseDefaultCategoryColors(QMap<QString, QString> &catColors);

  private:

    class UIGTCon
    {
      public:
        UIGTCon() { m_arrow = GridTimeNormal; m_recType = m_recStat = 0; };
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

        UIGTCon& operator=(const UIGTCon&) = default;

        QRect m_drawArea;
        QString m_title;
        QString m_category;
        QColor m_categoryColor;
        int m_arrow;
        int m_recType;
        int m_recStat;
    };

    void drawBackground(MythPainter *p, int xoffset, int yoffset,
                        UIGTCon *data, int alphaMod);
    void drawBox(MythPainter *p, int xoffset, int yoffset, UIGTCon *data,
                 const QColor &color, int alphaMod);
    void drawText(MythPainter *p, int xoffset, int yoffset, UIGTCon *data,
                  int alphaMod);
    void drawRecDecoration(MythPainter *p, int xoffset, int yoffset,
                           UIGTCon *data, int alphaMod);
    void drawCurrent(MythPainter *p, int xoffset, int yoffset, UIGTCon *data,
                     int alphaMod);

    static QColor calcColor(const QColor &color, int alpha);

    QList<UIGTCon*> *m_allData  {nullptr};
    UIGTCon m_selectedItem;

    MythUIImage *m_recImages[RECSTATUSSIZE]    {};
    MythUIImage *m_arrowImages[ARROWIMAGESIZE] {};

    // themeable settings
    int    m_channelCount       {5};
    int    m_timeCount          {4};
    bool   m_verticalLayout     {false};
    QPoint m_textOffset;

    MythFontProperties *m_font  {nullptr};
    int    m_justification      {Qt::AlignLeft | Qt::AlignTop |
                                 Qt::TextWordWrap};
    bool   m_multilineText      {true};
    bool   m_cutdown            {true};

    QString m_selType           {"box"};
    QPen    m_drawSelLine       {Qt::NoPen};
    QBrush  m_drawSelFill       {Qt::NoBrush};

    QColor m_solidColor;
    QColor m_recordingColor;
    QColor m_conflictingColor;

    int    m_fillType           {Solid};

    int  m_rowCount             {0};
    int  m_progPastCol          {0};

    bool   m_drawCategoryColors {true};
    bool   m_drawCategoryText   {true};
    int    m_categoryAlpha      {255};

    QMap<QString, QColor> m_categoryColors;

};

#endif
