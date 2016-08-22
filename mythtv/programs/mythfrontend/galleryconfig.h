//! \file
//! \brief Provides Gallery configuration screens

#ifndef GALLERYCONFIG_H
#define GALLERYCONFIG_H

#include "mythconfigdialogs.h"
#include "mythconfiggroups.h"


//! Settings page 1
class GallerySettings : public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    GallerySettings();
};


//! Settings page 2
class GalleryDbSettings : public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    explicit GalleryDbSettings(bool enable);

signals:
    void ClearDbPressed();
};


//! Gallery Settings pages
class GalleryConfig : public ConfigurationWizard
{
public:
    explicit GalleryConfig(bool editMode)
    {
        addChild(new GallerySettings());
        m_dbGroup = new GalleryDbSettings(editMode);
        addChild(m_dbGroup);
    }

    GalleryDbSettings *GetClearPage() { return m_dbGroup; }

private:
    GalleryDbSettings *m_dbGroup;
};


//! Settings for Thumbnail view
class ThumbSettings : public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    ThumbSettings();
};


//! Settings for Slideshow view
class SlideSettings : public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    SlideSettings();
};


//! Settings for Import 
class ImportSettings : public VerticalConfigurationGroup
{
    Q_OBJECT
public:
    explicit ImportSettings(bool enable);
};

#endif // GALLERYCONFIG_H
