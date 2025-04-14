#ifndef MYTHUIGUIDEGRID_H_
#define MYTHUIGUIDEGRID_H_

#include <array>
#include <utility>

// QT
#include <QBrush>
#include <QPen>
#include <QPixmap>

// MythDB
#include "mythuiexp.h"

// MythUI
#include "mythuitype.h"
#include "mythuiimage.h"

static constexpr size_t ARROWIMAGESIZE { 4 };
static constexpr size_t RECSTATUSSIZE  { 8 };


// max number of channels to display in the guide grid
static constexpr int MAX_DISPLAY_CHANS { 40 };

// max number of 5 minute time slots to show in guide grid (48 = 4hrs)
static constexpr int MAX_DISPLAY_TIMES { 48 };

// bitmask
static constexpr uint8_t GridTimeNormal       { 0 };
static constexpr uint8_t GridTimeStartsBefore { 1 };
static constexpr uint8_t GridTimeEndsAfter    { 2 };

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
    Q_OBJECT
  public:
    MythUIGuideGrid(MythUIType *parent, const QString &name);
    ~MythUIGuideGrid() override;

    void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                  int alphaMod, QRect clipRect) override; // MythUIType

    enum FillType : std::uint8_t { Alpha = 10, Dense, Eco, Solid };

    bool isVerticalLayout(void) const { return m_verticalLayout; }
    int  getChannelCount(void) const { return m_channelCount; }
    int  getTimeCount(void) const { return m_timeCount; }

    void SetCategoryColors(const QMap<QString, QString> &catColors);

    void SetTextOffset(const QPoint to) { m_textOffset = to; }
    void SetArrow(int direction, const QString &file);
    void LoadImage(int recType, const QString &file);
    void SetProgramInfo(int row, int col, QRect area,
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
        UIGTCon() = default;
        UIGTCon(const QRect drawArea, QString title,
                const QString &category, int arrow, int recType, int recStat) :
                m_drawArea(drawArea),               m_title(std::move(title)),
                m_category(category.trimmed()),     m_arrow(arrow),
                m_recType(recType),                 m_recStat(recStat)
        {}

        UIGTCon(const UIGTCon &o) = default;
        UIGTCon& operator=(const UIGTCon&) = default;

        QRect m_drawArea;
        QString m_title;
        QString m_category;
        QColor m_categoryColor;
        int m_arrow   { GridTimeNormal };
        int m_recType { 0 };
        int m_recStat { 0 };
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

    std::array<MythUIImage*,RECSTATUSSIZE> m_recImages    {};
    std::array<MythUIImage*,ARROWIMAGESIZE> m_arrowImages {};

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
