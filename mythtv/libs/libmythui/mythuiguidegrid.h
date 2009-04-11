#ifndef MYTHUIGUIDEGRID_H_
#define MYTHUIGUIDEGRID_H_

#include <QPixmap>

class MythFontProperties;

class MPUBLIC MythUIGuideGrid : public MythUIType
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

  protected:
    virtual void Finalize(void);
    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    bool parseDefaultCategoryColors(QMap<QString, QString> &catColors);

  private:

    class UIGTCon
    {
      public:
        UIGTCon() { arrow = recType = recStat = 0; };
        UIGTCon(const QRect &drawArea, const QString &title,
                const QString &category, int arrow, int recType, int recStat)
        {
            this->drawArea = drawArea;
            this->title = title;
            this->category = category.trimmed();
            this->arrow = arrow;
            this->recType = recType;
            this->recStat = recStat;
        }

        UIGTCon(const UIGTCon &o)
        {
            drawArea = o.drawArea;
            title = o.title;
            category = o.category;
            categoryColor = o.categoryColor;
            arrow = o.arrow;
            recType = o.recType;
            recStat = o.recStat;
        }

        QRect drawArea;
        QString title;
        QString category;
        QColor categoryColor;
        int arrow;
        int recType;
        int recStat;
    };

    void drawBackground(MythPainter *p, UIGTCon *data);
    void drawBox(MythPainter *p, UIGTCon *data, const QColor &color);
    void drawText(MythPainter *p, UIGTCon *data);
    void drawRecType(MythPainter *p, UIGTCon *data);
    void drawCurrent(MythPainter *p, UIGTCon *data);

    QList<UIGTCon*> *allData;
    UIGTCon selectedItem;

    QPixmap m_recImages[8];
    QPixmap m_arrowImages[4];

    // themeable settings
    int  m_channelCount;
    int  m_timeCount;
    bool m_verticalLayout;
    int  m_categoryAlpha;
    QPoint m_textOffset;
    int    m_justification;
    bool   m_multilineText;
    MythFontProperties *m_font;
    QColor m_solidColor;

    QString m_selType;
    QColor  m_selLineColor;
    QColor  m_selFillColor;
    bool    m_drawSelLine;
    bool    m_drawSelFill;

    QColor m_recordingColor;
    QColor m_conflictingColor;

    int    m_fillType;
    bool   m_cutdown;
    bool   m_drawCategoryColors;
    bool   m_drawCategoryText;

    QMap<QString, QColor> categoryColors;

    int  m_rowCount;
    int  m_progPastCol;
};

#endif
