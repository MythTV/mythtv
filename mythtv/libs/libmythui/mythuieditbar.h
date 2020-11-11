#ifndef MYTHUI_EDITBAR_H_
#define MYTHUI_EDITBAR_H_

#include "mythuitype.h"

class MythUIImage;
class MythUIShape;

/** \class MythUIEditBar
 *
 * \brief A narrow purpose widget used to represent cut positions and regions
 *        when editing a video.
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIEditBar : public MythUIType
{
  public:
    MythUIEditBar(MythUIType *parent, const QString &name)
        : MythUIType(parent, name) {}
   ~MythUIEditBar() override = default;

    void SetTotal(double total);
    void SetEditPosition(double position);
    void SetEditPosition(long long position);
    void AddRegion(double start, double end);
    void SetTotal(long long total);
    void AddRegion(long long start, long long end);
    void ClearRegions(void);
    void Display(void);
    void ReleaseImages(void);

  protected:
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    void Finalize(void) override; // MythUIType

  private:

    void AddBar(MythUIShape *shape, MythUIImage *image, QRect area);
    void AddMark(MythUIShape *shape, MythUIImage *image, int start, bool left);
    MythUIType* GetNew(MythUIShape *shape, MythUIImage *image);
    void CalcInverseRegions(void);
    void ClearImages(void);

    float  m_editPosition {0.0F};
    double m_total        {1.0};
    QList<QPair<float,float> > m_regions;
    QList<QPair<float,float> > m_invregions;
    QList<MythUIType*> m_images;
};

#endif
