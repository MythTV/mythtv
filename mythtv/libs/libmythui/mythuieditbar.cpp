#include "mythlogging.h"
#include "mythuieditbar.h"

MythUIEditBar::MythUIEditBar(MythUIType *parent, const QString &name)
    : MythUIType(parent, name), m_editPosition(0.0), m_total(1.0)
{
}

MythUIEditBar::~MythUIEditBar(void)
{
    ReleaseImages();
}

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

    if (newpos < 0.0f || newpos > 1.0f)
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
        int newx   = (int)(((float)keeparea.width() * m_editPosition) + 0.5f);
        int newy   = position->GetArea().top();
        position->SetPosition(newx - offset, newy);
        position->SetVisible(true);
    }

    ClearImages();

    if (!m_regions.size())
    {
        if (keep)
            keep->SetVisible(true);

        return;
    }

    MythUIShape *barshape   = dynamic_cast<MythUIShape *>(cut);
    MythUIImage *barimage   = dynamic_cast<MythUIImage *>(cut);
    MythUIShape *leftshape  = dynamic_cast<MythUIShape *>(cuttoleft);
    MythUIImage *leftimage  = dynamic_cast<MythUIImage *>(cuttoleft);
    MythUIShape *rightshape = dynamic_cast<MythUIShape *>(cuttoright);
    MythUIImage *rightimage = dynamic_cast<MythUIImage *>(cuttoright);

    QListIterator<QPair<float, float> > regions(m_regions);

    while (regions.hasNext() && cutarea.isValid())
    {
        QPair<float, float> region = regions.next();
        int left  = (int)((region.first * cutarea.width()) + 0.5f);
        int right = (int)((region.second * cutarea.width()) + 0.5f);

        if (left >= right)
            right = left + 1;

        if (cut)
        {
            AddBar(barshape, barimage, QRect(left, cutarea.top(), right - left,
                                             cutarea.height()));
        }

        if (cuttoleft && (region.second < 1.0f))
            AddMark(leftshape, leftimage, right, true);

        if (cuttoright && (region.first > 0.0f))
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
        int left  = (int)((region.first * keeparea.width()) + 0.5f);
        int right = (int)((region.second * keeparea.width()) + 0.5f);

        if (left >= right)
            right = left + 1;

        if (keep)
        {
            AddBar(barshape, barimage, QRect(left, keeparea.top(), right - left,
                                             keeparea.height()));
        }

        if (keeptoleft && (region.second < 1.0f))
            AddMark(leftshape, leftimage, right, true);

        if (keeptoright && (region.first > 0.0f))
            AddMark(rightshape, rightimage, left, false);
    }

    if (position)
        position->MoveToTop();
}

void MythUIEditBar::AddBar(MythUIShape *shape, MythUIImage *image,
                           const QRect &area)
{
    MythUIType *add = GetNew(shape, image);

    if (add)
    {
        MythUIShape *shape = dynamic_cast<MythUIShape *>(add);
        MythUIImage *image = dynamic_cast<MythUIImage *>(add);

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
        MythUIShape *newshape = new MythUIShape(this, name);

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
        MythUIImage *newimage = new MythUIImage(this, name);

        if (newimage)
        {
            newimage->CopyFrom(image);
            newimage->SetVisible(true);
            m_images.append(newimage);
            return newimage;
        }
    }

    return NULL;
}

void MythUIEditBar::CalcInverseRegions(void)
{
    m_invregions.clear();

    bool first = true;
    float start = 0.0f;
    QListIterator<QPair<float, float> > regions(m_regions);

    while (regions.hasNext())
    {
        QPair<float, float> region = regions.next();

        if (first)
        {
            if (region.first > 0.0f)
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

    if (start < 1.0f)
        m_invregions.append(qMakePair(start, 1.0f));
}

void MythUIEditBar::ClearImages(void)
{
    while (m_images.size())
        DeleteChild(m_images.takeFirst());
    SetRedraw();
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIEditBar::CopyFrom(MythUIType *base)
{
    MythUIEditBar *editbar = dynamic_cast<MythUIEditBar *>(base);

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
    MythUIEditBar *editbar = new MythUIEditBar(parent, objectName());
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
