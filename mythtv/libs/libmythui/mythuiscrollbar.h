#ifndef MYTHUI_SCROLLBAR_H_
#define MYTHUI_SCROLLBAR_H_

#include "mythuitype.h"

const int kDefaultMaxValue = 99;
const int kDefaultPageStep = 10;

/** \class MythUIScrollBar
 *
 * \brief Scroll bar widget.
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIScrollBar : public MythUIType
{
    Q_OBJECT

  public:
    MythUIScrollBar(MythUIType *parent, const QString &name)
        : MythUIType(parent, name) {}
   ~MythUIScrollBar() = default;

    void Reset(void) override; // MythUIType

    enum LayoutType { LayoutVertical, LayoutHorizontal };

    void SetPageStep(int);
    void SetSliderPosition(int);
    void SetMaximum(int);

  protected slots:
    void DoneFading(void);

  protected:
    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    void Finalize(void) override; // MythUIType

    void CalculatePosition(void);
    void timerEvent(QTimerEvent *) override; // QObject

    LayoutType m_layout   {LayoutVertical};

    MythRect m_sliderArea;
    int m_pageStep        {kDefaultPageStep};
    int m_sliderPosition  {0};
    int m_maximum         {0};

    int m_hideDelay       {0};
    int m_timerId         {0};
};

#endif
