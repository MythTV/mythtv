// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _DISH_DESCRIPTORS_H_
#define _DISH_DESCRIPTORS_H_

#include <cassert>
#include <map>

#include <qmutex.h>
#include <QString>

#include "atscdescriptors.h"

class DishEventMPAADescriptor : public MPEGDescriptor
{
  public:
    DishEventMPAADescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x89
        assert(DescriptorID::dish_event_mpaa == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // stars                    3   2.0
    uint stars_raw(void) const { return (_data[2] & 0xe0) >> 0x05; }
    float stars(void) const;

    // rating                   3   2.3
    uint rating_raw(void) const { return (_data[2] & 0x1c) >> 0x02; }
    QString rating(void) const;

    // advisories               8   3.0
    uint advisory_raw(void) const { return _data[3]; }
    QString advisory(void) const;

  private:
    static void Init(void);

  private:
    static QMutex            mpaaRatingsLock;
    static map<uint,QString> mpaaRatingsDesc;
    static bool              mpaaRatingsExists;
};

class DishEventVCHIPDescriptor : public MPEGDescriptor
{
  public:
    DishEventVCHIPDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)               
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x95
        assert(DescriptorID::dish_event_vchip == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // rating                   8   2.0
    uint rating_raw(void) const { return _data[2]; }
    QString rating(void) const;

    // advisory                 8   3.0
    uint advisory_raw(void) const { return _data[3]; }
    QString advisory(void) const;

  private:
    static void Init(void);

  private:
    static QMutex            vchipRatingsLock;
    static map<uint,QString> vchipRatingsDesc;
    static bool              vchipRatingsExists;
};

class DishEventNameDescriptor : public MPEGDescriptor
{
  public:
    DishEventNameDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x91
        assert(DescriptorID::dish_event_name == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // unknown                  8   2.0
    // event_name            dlen-1 3.0
    bool HasName(void) const { return DescriptorLength() > 1; }
    QString Name(uint) const;
};

class DishEventDescriptionDescriptor : public MPEGDescriptor
{
  public:
    DishEventDescriptionDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x92
        assert(DescriptorID::dish_event_description == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // unknown                 8/16 2.0
    // event_name            dlen-2 3.0/4.0
    const unsigned char *DescriptionRaw(void) const;
    uint DescriptionRawLength(void) const;
    bool HasDescription(void) const { return DescriptionRawLength(); }
    QString Description(uint) const;
};

class DishEventPropertiesDescriptor : public MPEGDescriptor
{
  public:
    DishEventPropertiesDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x94
        assert(DescriptorID::dish_event_properties == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // unknown                  8   2.0
    // event_name            dlen-1 3.0
    bool HasProperties(void) const { return DescriptorLength() > 1; }
    uint SubtitleProperties(uint compression_type) const;
    uint AudioProperties(uint compression_type) const;

  private:
    void decompress_properties(uint compression_type) const;

  private:
    static uint subtitle_props;
    static uint audio_props;
    static bool decompressed;
};

class DishEventTagsDescriptor : public MPEGDescriptor
{
  public:
    DishEventTagsDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x96
        assert(DescriptorID::dish_event_tags == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // unknown                  8   2.0
    // seriesid                 28  3.0
    QString seriesid(void) const;

    // programid                42  3.0
    QString programid(void) const;
};

typedef enum
{
    kThemeNone = 0,
    kThemeMovie,
    kThemeSports,
    kThemeNews,
    kThemeFamily,
    kThemeEducation,
    kThemeSeries,
    kThemeMusic,
    kThemeReligious,
    kThemeOffAir,
    kThemeLast,
} DishThemeType;

QString dish_theme_type_to_string(uint category_type);
DishThemeType string_to_dish_theme_type(const QString &type);

class DishContentDescriptor : public MPEGDescriptor
{
  public:
    DishContentDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x54
        assert(DescriptorID::content == DescriptorTag());
    // descriptor_length        8   1.0
    }

    //   content_nibble_level_1 4   0.0
    uint Nibble1(void)       const { return _data[2] >> 4; }
    //   content_nibble_level_2 4   0.4
    uint Nibble2(void)       const { return _data[2] & 0xf; }

    uint Nibble(void)        const { return _data[2]; }

    //   user_nibble            4   1.0
    uint UserNibble1(void)   const { return _data[3] >> 4; }
    //   user_nibble            4   1.4
    uint UserNibble2(void)   const { return _data[3] & 0xf; }
    uint UserNibble(void)    const { return _data[3]; }
    // }                            2.0

    DishThemeType GetTheme(void) const;
    QString GetCategory(void) const;
    QString toString() const;

  private:
    static void Init(void);

  private:
    static QMutex            categoryLock;
    static map<uint,QString> themeDesc;
    static map<uint,QString> categoryDesc;
    static bool              categoriesExists;
};

#endif // _DISH_DESCRIPTORS_H_
