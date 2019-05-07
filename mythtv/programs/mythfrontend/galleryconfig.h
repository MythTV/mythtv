//! \file
//! \brief Provides Gallery configuration screens

#ifndef GALLERYCONFIG_H
#define GALLERYCONFIG_H

#include "standardsettings.h"

class GallerySettings : public GroupSetting
{
    Q_OBJECT

    StandardSetting *DirOrder();
    StandardSetting *ImageOrder();
    StandardSetting *DateFormat();
    StandardSetting *Exclusions (bool enabled);
    StandardSetting *ClearDb    (bool enabled);
    void             ShowConfirmDialog();

signals:
    void ClearDbPressed();
    void DateChanged();
    void OrderChanged();
    void ExclusionsChanged();

public:
    explicit GallerySettings(bool enable);
};

#endif // GALLERYCONFIG_H
