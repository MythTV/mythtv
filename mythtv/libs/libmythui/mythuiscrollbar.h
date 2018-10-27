#ifndef MYTHUI_SCROLLBAR_H_
#define MYTHUI_SCROLLBAR_H_

#include "mythuitype.h"

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
    MythUIScrollBar(MythUIType *parent, const QString &name);
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

    LayoutType m_layout;

    MythRect m_sliderArea;
    int m_pageStep;
    int m_sliderPosition;
    int m_maximum;

    int m_hideDelay;
    int m_timerId;
};

#endif
