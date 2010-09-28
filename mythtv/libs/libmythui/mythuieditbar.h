#ifndef MYTHUI_EDITBAR_H_
#define MYTHUI_EDITBAR_H_

#include "mythuitype.h"
#include "mythuiimage.h"
#include "mythuishape.h"

/** \class MythUIEditBar
 *
 * \brief A narrow purpose widget used to represent cut positions and regions
 *        when editing a video.
 *
 * \ingroup MythUI_Widgets
 */
class MPUBLIC MythUIEditBar : public MythUIType
{
  public:
    MythUIEditBar(MythUIType *parent, const QString &name);
   ~MythUIEditBar();

    void SetTotal(double total);
    void SetPosition(double position);
    void AddRegion(double start, double end);
    void SetTotal(long long total);
    void SetPosition(long long position);
    void AddRegion(long long start, long long end);
    void ClearRegions(void);
    void Display(void);
    void ReleaseImages(void);

  protected:
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

  private:

    void AddBar(MythUIShape *shape, MythUIImage *image, const QRect &area);
    void AddMark(MythUIShape *shape, MythUIImage *image, int start, bool left);
    MythUIType* GetNew(MythUIShape *shape, MythUIImage *image);
    void CalcInverseRegions(void);
    void ClearImages(void);

    float  m_position;
    double m_total;
    QList<QPair<float,float> > m_regions;
    QList<QPair<float,float> > m_invregions;
    QList<MythUIType*> m_images;
};

#endif
