
#include "mythuieditbar.h"

// C++
#include <cmath>

// MythBase
#include "libmythbase/mythlogging.h"

// MythUI
#include "mythuishape.h"
#include "mythuiimage.h"

void MythUIEditBar::ReleaseImages(void)
{
    ClearImages();
}

void MythUIEditBar::SetTotal(double total)
{
    if (total < 1.0)
        return;

    bool changed = m_total != total;
    m_total = total;

    if (changed)
        Display();
}

void MythUIEditBar::SetEditPosition(double position)
{
    float newpos = position / m_total;

    if (newpos < 0.0F || newpos > 1.0F)
        return;

    bool changed = m_editPosition != newpos;
    m_editPosition = newpos;

    if (changed)
        Display();
}

void MythUIEditBar::AddRegion(double start, double end)
{
    if (m_total >= end && end >= start && start >= 0.0)
        m_regions.append(qMakePair((float)(start / m_total),
                                   (float)(end   / m_total)));
}

void MythUIEditBar::SetTotal(long long total)
{
    SetTotal((double)total);
}

void MythUIEditBar::SetEditPosition(long long position)
{
    SetEditPosition((double)position);
}

void MythUIEditBar::AddRegion(long long start, long long end)
{
    AddRegion((double)start, (double)end);
}

void MythUIEditBar::ClearRegions(void)
{
    m_regions.clear();
}

void MythUIEditBar::Display(void)
{
    QRect keeparea = QRect();
    QRect cutarea  = QRect();
    MythUIType *position    = GetChild("position");
    MythUIType *keep        = GetChild("keep");
    MythUIType *cut         = GetChild("cut");
    MythUIType *cuttoleft   = GetChild("cuttoleft");
    MythUIType *cuttoright  = GetChild("cuttoright");
    MythUIType *keeptoleft  = GetChild("keeptoleft");
    MythUIType *keeptoright = GetChild("keeptoright");

    if (position)
        position->SetVisible(false);

    if (keep)
    {
        keep->SetVisible(false);
        keeparea = keep->GetArea();
    }

    if (cut)
    {
        cut->SetVisible(false);
        cutarea = cut->GetArea();
    }

    if (cuttoleft)
        cuttoleft->SetVisible(false);

    if (cuttoright)
        cuttoright->SetVisible(false);

    if (keeptoleft)
        keeptoleft->SetVisible(false);

    if (keeptoright)
        keeptoright->SetVisible(false);

    if (position && keeparea.isValid())
    {
        int offset = position->GetArea().width() / 2;
        int newx   = lroundf((float)keeparea.width() * m_editPosition);
        int newy   = position->GetArea().top();
        position->SetPosition(newx - offset, newy);
        position->SetVisible(true);
    }

    ClearImages();

    if (m_regions.isEmpty())
    {
        if (keep)
            keep->SetVisible(true);

        return;
    }

    auto *barshape   = dynamic_cast<MythUIShape *>(cut);
    auto *barimage   = dynamic_cast<MythUIImage *>(cut);
    auto *leftshape  = dynamic_cast<MythUIShape *>(cuttoleft);
    auto *leftimage  = dynamic_cast<MythUIImage *>(cuttoleft);
    auto *rightshape = dynamic_cast<MythUIShape *>(cuttoright);
    auto *rightimage = dynamic_cast<MythUIImage *>(cuttoright);

    QListIterator<QPair<float, float> > regions(m_regions);

    while (regions.hasNext() && cutarea.isValid())
    {
        QPair<float, float> region = regions.next();
        int left  = lroundf(region.first * cutarea.width());
        int right = lroundf(region.second * cutarea.width());

        if (left >= right)
            right = left + 1;

        if (cut)
        {
            AddBar(barshape, barimage, QRect(left, cutarea.top(), right - left,
                                             cutarea.height()));
        }

        if (cuttoleft && (region.second < 1.0F))
            AddMark(leftshape, leftimage, right, true);

        if (cuttoright && (region.first > 0.0F))
            AddMark(rightshape, rightimage, left, false);
    }

    CalcInverseRegions();

    barshape   = dynamic_cast<MythUIShape *>(keep);
    barimage   = dynamic_cast<MythUIImage *>(keep);
    leftshape  = dynamic_cast<MythUIShape *>(keeptoleft);
    leftimage  = dynamic_cast<MythUIImage *>(keeptoleft);
    rightshape = dynamic_cast<MythUIShape *>(keeptoright);
    rightimage = dynamic_cast<MythUIImage *>(keeptoright);

    QListIterator<QPair<float, float> > regions2(m_invregions);

    while (regions2.hasNext() && keeparea.isValid())
    {
        QPair<float, float> region = regions2.next();
        int left  = lroundf(region.first * keeparea.width());
        int right = lroundf(region.second * keeparea.width());

        if (left >= right)
            right = left + 1;

        if (keep)
        {
            AddBar(barshape, barimage, QRect(left, keeparea.top(), right - left,
                                             keeparea.height()));
        }

        if (keeptoleft && (region.second < 1.0F))
            AddMark(leftshape, leftimage, right, true);

        if (keeptoright && (region.first > 0.0F))
            AddMark(rightshape, rightimage, left, false);
    }

    if (position)
        position->MoveToTop();
}

void MythUIEditBar::AddBar(MythUIShape *_shape, MythUIImage *_image,
                           const QRect area)
{
    MythUIType *add = GetNew(_shape, _image);

    if (add)
    {
        auto *shape = dynamic_cast<MythUIShape *>(add);
        auto *image = dynamic_cast<MythUIImage *>(add);

        if (shape)
            shape->SetCropRect(area.left(), area.top(), area.width(), area.height());

        if (image)
            image->SetCropRect(area.left(), area.top(), area.width(), area.height());

        add->SetPosition(area.left(), area.top());
    }
}

void MythUIEditBar::AddMark(MythUIShape *shape, MythUIImage *image,
                            int start, bool left)
{
    MythUIType *add = GetNew(shape, image);

    if (add)
    {
        if (left)
            start -= add->GetArea().width();

        add->SetPosition(start, add->GetArea().top());
    }
}

MythUIType *MythUIEditBar::GetNew(MythUIShape *shape, MythUIImage *image)
{
    QString name = QString("editbarimage_%1").arg(m_images.size());

    if (shape)
    {
        auto *newshape = new MythUIShape(this, name);

        if (newshape)
        {
            newshape->CopyFrom(shape);
            newshape->SetVisible(true);
            m_images.append(newshape);
            return newshape;
        }
    }
    else if (image)
    {
        auto *newimage = new MythUIImage(this, name);

        if (newimage)
        {
            newimage->CopyFrom(image);
            newimage->SetVisible(true);
            m_images.append(newimage);
            return newimage;
        }
    }

    return nullptr;
}

void MythUIEditBar::CalcInverseRegions(void)
{
    m_invregions.clear();

    bool first = true;
    float start = 0.0F;
    QListIterator<QPair<float, float> > regions(m_regions);

    while (regions.hasNext())
    {
        QPair<float, float> region = regions.next();

        if (first)
        {
            if (region.first > 0.0F)
                m_invregions.append(qMakePair(start, region.first));

            start = region.second;
            first = false;
        }
        else
        {
            m_invregions.append(qMakePair(start, region.first));
            start = region.second;
        }
    }

    if (start < 1.0F)
        m_invregions.append(qMakePair(start, 1.0F));
}

void MythUIEditBar::ClearImages(void)
{
    while (!m_images.empty())
        DeleteChild(m_images.takeFirst());
    SetRedraw();
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIEditBar::CopyFrom(MythUIType *base)
{
    auto *editbar = dynamic_cast<MythUIEditBar *>(base);

    if (!editbar)
        return;

    m_editPosition = editbar->m_editPosition;

    QListIterator<QPair<float, float> > it(m_regions);

    while (it.hasNext())
        editbar->m_regions.append(it.next());

    MythUIType::CopyFrom(base);
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIEditBar::CreateCopy(MythUIType *parent)
{
    auto *editbar = new MythUIEditBar(parent, objectName());
    editbar->CopyFrom(this);
}

/**
 *  \copydoc MythUIType::Finalize()
 */
void MythUIEditBar::Finalize(void)
{
    MythUIType *position = GetChild("position");

    if (position)
        position->MoveToTop();
}
