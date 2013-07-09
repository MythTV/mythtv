#ifndef MYTHUI_PROGRESSBAR_H_
#define MYTHUI_PROGRESSBAR_H_

#include "mythuitype.h"
#include "mythuiimage.h"
#include "mythuishape.h"

/** \class MythUIProgressBar
 *
 * \brief Progress bar widget.
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIProgressBar : public MythUIType
{
  public:
    MythUIProgressBar(MythUIType *parent, const QString &name);
   ~MythUIProgressBar() { }

    void Reset(void);

    enum LayoutType { LayoutVertical, LayoutHorizontal };
    enum EffectType { EffectReveal, EffectSlide, EffectAnimate };

    void SetStart(int);
    void SetUsed(int);
    void SetTotal(int);
    int  GetUsed(void) { return m_current; }
    virtual void SetVisible(bool visible);

  protected:
    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    LayoutType m_layout;
    EffectType m_effect;

    int m_total;
    int m_start;
    int m_current;
    int m_firstdepend;

    void CalculatePosition(void);
};

#endif
