//! \file
//! \brief Provides Gallery configuration screens
//!

#ifndef GALLERYCONFIG_H
#define GALLERYCONFIG_H

#include <mythconfigdialogs.h>
#include <mythconfiggroups.h>
#include "gallerycommhelper.h"


//! Settings page 1
class GallerySettings : public VerticalConfigurationGroup
{
public:
    GallerySettings();
};


//! Settings page 2
class DatabaseSettings : public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    DatabaseSettings(bool enable);

private slots:
    void ClearDb()    { GalleryBERequest::ClearDatabase(); }
};


//! Gallery Settings pages
class GalleryConfig : public ConfigurationWizard
{
public:
    GalleryConfig(bool editMode)
    {
        addChild(new GallerySettings());
        addChild(new DatabaseSettings(editMode));
    }
};


#endif // GALLERYCONFIG_H
