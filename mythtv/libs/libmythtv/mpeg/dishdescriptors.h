// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _DISH_DESCRIPTORS_H_
#define _DISH_DESCRIPTORS_H_

#include <QString>
#include <QMutex>
#include <QDate>
#include <QMap>

#include "atscdescriptors.h"
#include "dvbdescriptors.h"

class DishEventMPAADescriptor : public MPEGDescriptor
{
  public:
    DishEventMPAADescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::dish_event_mpaa) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x89
    // descriptor_length        8   1.0
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
    static QMutex             mpaaRatingsLock;
    static QMap<uint,QString> mpaaRatingsDesc;
    static bool               mpaaRatingsExists;
};

class DishEventVCHIPDescriptor : public MPEGDescriptor
{
  public:
    DishEventVCHIPDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::dish_event_vchip) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x95
    // descriptor_length        8   1.0
    // rating                   8   2.0
    uint rating_raw(void) const { return _data[2]; }
    QString rating(void) const;

    // advisory                 8   3.0
    uint advisory_raw(void) const { return _data[3]; }
    QString advisory(void) const;

  private:
    static void Init(void);

  private:
    static QMutex             vchipRatingsLock;
    static QMap<uint,QString> vchipRatingsDesc;
    static bool               vchipRatingsExists;
};

class DishEventNameDescriptor : public MPEGDescriptor
{
  public:
    DishEventNameDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::dish_event_name) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x91
    // descriptor_length        8   1.0
    // unknown                  8   2.0
    // event_name            dlen-1 3.0
    bool HasName(void) const { return DescriptorLength() > 1; }
    QString Name(uint) const;
};

class DishEventDescriptionDescriptor : public MPEGDescriptor
{
  public:
    DishEventDescriptionDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(
            data, len, PrivateDescriptorID::dish_event_description) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x92
    // descriptor_length        8   1.0
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
    DishEventPropertiesDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(
            data, len, PrivateDescriptorID::dish_event_properties) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x94
    // descriptor_length        8   1.0
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
    DishEventTagsDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::dish_event_tags) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x96
    // descriptor_length        8   1.0
    // seriesid                 64  2.0
    QString seriesid(void) const;
    QString programid(void) const;
    QDate originalairdate(void) const;
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

class DishContentDescriptor : public ContentDescriptor
{
  public:
    DishContentDescriptor(const unsigned char *data, int len = 300) :
        ContentDescriptor(data, len) { }

    DishThemeType GetTheme(void) const;
    QString GetCategory(void) const;
    QString toString() const;

  private:
    static void Init(void);

  private:
    static QMap<uint,QString> themeDesc;
    static QMap<uint,QString> dishCategoryDesc;
    static volatile bool      dishCategoryDescExists;
};

#endif // _DISH_DESCRIPTORS_H_
