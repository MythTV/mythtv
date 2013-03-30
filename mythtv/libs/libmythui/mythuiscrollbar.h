#ifndef MYTHUI_SCROLLBAR_H_
#define MYTHUI_SCROLLBAR_H_

#include "mythuitype.h"
#include "mythuiimage.h"
#include "mythuishape.h"

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
   ~MythUIScrollBar();

    void Reset(void);

    enum LayoutType { LayoutVertical, LayoutHorizontal };

    void SetPageStep(int);
    void SetSliderPosition(int);
    void SetMaximum(int);

  protected slots:
    void DoneFading(void);

  protected:
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    void CalculatePosition(void);
    void timerEvent(QTimerEvent *);

    LayoutType m_layout;

    MythRect m_sliderArea;
    int m_pageStep;
    int m_sliderPosition;
    int m_maximum;

    int m_hideDelay;
    int m_timerId;
};

#endif
