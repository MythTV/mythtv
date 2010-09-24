// -*- Mode: c++ -*-

#include "dishdescriptors.h"
#include "atsc_huffman.h"
#include "programinfo.h" // for subtitle types and audio and video properties

QString DishEventNameDescriptor::Name(uint compression_type) const
{
    if (!HasName())
        return QString::null;

    return atsc_huffman2_to_string(
        _data + 3, DescriptorLength() - 1, compression_type);
}

const unsigned char *DishEventDescriptionDescriptor::DescriptionRaw(void) const
{
    if (DescriptorLength() <= 2)
        return NULL;

    bool offset = (_data[3] & 0xf8) == 0x80;
    return _data + ((offset) ? 4 : 3);
}

uint DishEventDescriptionDescriptor::DescriptionRawLength(void) const
{
    if (DescriptorLength() <= 2)
        return 0;

    bool offset = (_data[3] & 0xf8) == 0x80;
    return DescriptorLength() - ((offset) ? 2 : 1);
}

QString DishEventDescriptionDescriptor::Description(
    uint compression_type) const
{
    const unsigned char *raw = DescriptionRaw();
    uint len = DescriptionRawLength();

    if (raw && len)
        return atsc_huffman2_to_string(raw, len, compression_type);

    return QString::null;
}

bool DishEventPropertiesDescriptor::decompressed = false;
uint DishEventPropertiesDescriptor::subtitle_props = SUB_UNKNOWN;
uint DishEventPropertiesDescriptor::audio_props = AUD_UNKNOWN;

uint DishEventPropertiesDescriptor::SubtitleProperties(uint compression_type) const
{
    decompress_properties(compression_type);

    return subtitle_props;
}

uint DishEventPropertiesDescriptor::AudioProperties(uint compression_type) const
{
    decompress_properties(compression_type);

    return audio_props;
}

void DishEventPropertiesDescriptor::decompress_properties(uint compression_type) const
{
    subtitle_props = SUB_UNKNOWN;
    audio_props = AUD_UNKNOWN;

    if (HasProperties())
    {
        QString properties_raw = atsc_huffman2_to_string(
            _data + 4, DescriptorLength() - 2, compression_type);

        if (properties_raw.contains("6|CC"))
            subtitle_props |= SUB_HARDHEAR;

        if (properties_raw.contains("7|Stereo"))
            audio_props    |= AUD_STEREO;
    }
}

QString DishEventTagsDescriptor::programid(void) const
{
    QString id;

    if (DescriptorLength() != 8)
        return QString::null;

    id.sprintf("%02x%02x%02x%02x%02x%02x%02x", _data[3], _data[4], _data[5],
                                               _data[6], _data[7], _data[8],
                                               _data[9]);

    return id;
}

QString DishEventTagsDescriptor::seriesid(void) const            
{
    QString id;

    if (DescriptorLength() != 8)
        return QString::null;

    id.sprintf("%02x%02x%02x%01x", _data[3], _data[4], _data[5],
                                   ((_data[6] & 0xf0) >> 0x04));

    return id;
}

QMutex            DishEventMPAADescriptor::mpaaRatingsLock;
map<uint,QString> DishEventMPAADescriptor::mpaaRatingsDesc;
bool              DishEventMPAADescriptor::mpaaRatingsExists = false;

float DishEventMPAADescriptor::stars(void) const
{
    switch(stars_raw())
    {
        case 0x01: return 1.0 / 4;
        case 0x02: return 1.5 / 4;
        case 0x03: return 2.0 / 4;
        case 0x04: return 2.5 / 4;
        case 0x05: return 3.0 / 4;
        case 0x06: return 3.5 / 4;
        case 0x07: return 4.0 / 4;
    }

    return 0.0;
}

QString DishEventMPAADescriptor::rating(void) const
{
    if (!mpaaRatingsExists)
        Init();

    QMutexLocker locker(&mpaaRatingsLock);

    map<uint,QString>::const_iterator it = mpaaRatingsDesc.find(rating_raw());
    if (it != mpaaRatingsDesc.end())
    {
        QString ret = (*it).second;
        ret.detach();
        return ret;
    }

    // Found nothing? Just return empty string.
    return "";
}

QString DishEventMPAADescriptor::advisory(void) const
{
    int advisory = advisory_raw();
    QStringList advisories;

    if (advisory & 0x01)
        advisories.append("S");
    if (advisory & 0x02)
        advisories.append("L");
    if (advisory & 0x04)
        advisories.append("mQ");
    if (advisory & 0x08)
        advisories.append("FV");
    if (advisory & 0x10)
        advisories.append("V");
    if (advisory & 0x20)
        advisories.append("mK");
    if (advisory & 0x40)
        advisories.append("N");

    return advisories.join(",");
}

void DishEventMPAADescriptor::Init(void)
{
    QMutexLocker locker(&mpaaRatingsLock);

    if (mpaaRatingsExists)
        return;

    mpaaRatingsDesc[0x01] = QObject::tr("G");
    mpaaRatingsDesc[0x02] = QObject::tr("PG");
    mpaaRatingsDesc[0x03] = QObject::tr("PG-13");
    mpaaRatingsDesc[0x04] = QObject::tr("R");
    mpaaRatingsDesc[0x05] = QObject::tr("NC-17");
    mpaaRatingsDesc[0x06] = QObject::tr("NR");

    mpaaRatingsExists = true;
}

QMutex            DishEventVCHIPDescriptor::vchipRatingsLock;
map<uint,QString> DishEventVCHIPDescriptor::vchipRatingsDesc;
bool              DishEventVCHIPDescriptor::vchipRatingsExists = false;

QString DishEventVCHIPDescriptor::rating(void) const
{
    if (!vchipRatingsExists)
        Init();

    QMutexLocker locker(&vchipRatingsLock);

    map<uint,QString>::const_iterator it = vchipRatingsDesc.find(rating_raw());
    if (it != vchipRatingsDesc.end())
    {
        QString ret = (*it).second;
        ret.detach();
        return ret;
    }

    // Found nothing? Just return empty string.
    return "";
}

QString DishEventVCHIPDescriptor::advisory(void) const
{
    int advisory = advisory_raw();
    QStringList advisories;

    if (advisory & 0x01)
        advisories.append("FV");
    if (advisory & 0x02)
        advisories.append("V");
    if (advisory & 0x04)
        advisories.append("S");
    if (advisory & 0x08)
        advisories.append("L");
    if (advisory & 0x10)
        advisories.append("D");

    return advisories.join(",");
}

void DishEventVCHIPDescriptor::Init(void)
{
    QMutexLocker locker(&vchipRatingsLock);

    if (vchipRatingsExists)
        return;

    vchipRatingsDesc[0x01] = QObject::tr("TV-Y");
    vchipRatingsDesc[0x02] = QObject::tr("TV-Y7");
    vchipRatingsDesc[0x03] = QObject::tr("TV-G");
    vchipRatingsDesc[0x04] = QObject::tr("TV-PG");
    vchipRatingsDesc[0x05] = QObject::tr("TV-14");
    vchipRatingsDesc[0x06] = QObject::tr("TV-MA");

    vchipRatingsExists = true;
}

QMutex            DishContentDescriptor::categoryLock;
map<uint,QString> DishContentDescriptor::themeDesc;
map<uint,QString> DishContentDescriptor::categoryDesc;
bool              DishContentDescriptor::categoriesExists = false;

QString dish_theme_type_to_string(uint theme_type)
{
    static const char *themes[] =
        { "", "Movie", "Sports", "News/Business", "Family/Children", "Education",
          "Series/Special", "Music/Art", "Religious", "Off-Air" };

    if ((theme_type > kThemeNone) && (theme_type < kThemeLast))
        return QString(themes[theme_type]);

    return "";
}

DishThemeType string_to_dish_theme_type(const QString &theme_type)
{
    static const char *themes[] =
        { "", "Movie", "Sports", "News/Business", "Family/Children", "Education",
          "Series/Special", "Music/Art", "Religious", "Off-Air" };

    for (uint i = 1; i < 10; i++)
        if (theme_type == themes[i])
            return (DishThemeType) i;

    return kThemeNone;
}

DishThemeType DishContentDescriptor::GetTheme(void) const
{
    if (!categoriesExists)
        Init();

    if (Nibble1() == 0x00)
        return kThemeOffAir;

    map<uint,QString>::const_iterator it = themeDesc.find(Nibble2());
    if (it != themeDesc.end())
        return string_to_dish_theme_type((*it).second);

    // Found nothing? Just return empty string.
    return kThemeNone;
}

QString DishContentDescriptor::GetCategory(void) const
{
    if (!categoriesExists)
        Init();

    QMutexLocker locker(&categoryLock);

    // Try to get detailed description
    map<uint,QString>::const_iterator it = categoryDesc.find(UserNibble());
    if (it != categoryDesc.end())
    {
        QString ret = (*it).second;
        ret.detach();
        return ret;
    }

    // Fallback to just the theme
    QString theme = dish_theme_type_to_string(GetTheme());

    if (theme != "")
        return theme;

    // Found nothing? Just return empty string.
    return "";
}

QString DishContentDescriptor::toString() const
{
    QString tmp("");
    tmp += GetTheme() + " : " + GetCategory();
    return tmp;
}

void DishContentDescriptor::Init(void)
{
    QMutexLocker locker(&categoryLock);

    if (categoriesExists)
        return;

    // Dish/BEV "Themes"
    themeDesc[kThemeMovie]     = QObject::tr("Movie");
    themeDesc[kThemeSports]    = QObject::tr("Sports");
    themeDesc[kThemeNews]      = QObject::tr("News/Business");
    themeDesc[kThemeFamily]    = QObject::tr("Family/Children");
    themeDesc[kThemeEducation] = QObject::tr("Education");
    themeDesc[kThemeSeries]    = QObject::tr("Series/Special");
    themeDesc[kThemeMusic]     = QObject::tr("Music/Art");
    themeDesc[kThemeReligious] = QObject::tr("Religious");

    // Dish/BEV categories
    categoryDesc[0x01] = QObject::tr("Action");
    categoryDesc[0x02] = QObject::tr("Adults only");
    categoryDesc[0x03] = QObject::tr("Adventure");
    categoryDesc[0x04] = QObject::tr("Animals");
    categoryDesc[0x05] = QObject::tr("Animated");
    categoryDesc[0x07] = QObject::tr("Anthology");
    categoryDesc[0x08] = QObject::tr("Art");
    categoryDesc[0x09] = QObject::tr("Auto");
    categoryDesc[0x0a] = QObject::tr("Awards");
    categoryDesc[0x0b] = QObject::tr("Ballet");
    categoryDesc[0x0c] = QObject::tr("Baseball");
    categoryDesc[0x0d] = QObject::tr("Basketball");
    categoryDesc[0x11] = QObject::tr("Biography");
    categoryDesc[0x12] = QObject::tr("Boat");
    categoryDesc[0x14] = QObject::tr("Bowling");
    categoryDesc[0x15] = QObject::tr("Boxing");
    categoryDesc[0x16] = QObject::tr("Bus./financial");
    categoryDesc[0x1a] = QObject::tr("Children");
    categoryDesc[0x1b] = QObject::tr("Children-special");
    categoryDesc[0x1c] = QObject::tr("Children-news");
    categoryDesc[0x1d] = QObject::tr("Children-music");
    categoryDesc[0x1f] = QObject::tr("Collectibles");
    categoryDesc[0x20] = QObject::tr("Comedy");
    categoryDesc[0x21] = QObject::tr("Comedy-drama");
    categoryDesc[0x22] = QObject::tr("Computers");
    categoryDesc[0x23] = QObject::tr("Cooking");
    categoryDesc[0x24] = QObject::tr("Crime");
    categoryDesc[0x25] = QObject::tr("Crime drama");
    categoryDesc[0x27] = QObject::tr("Dance");
    categoryDesc[0x29] = QObject::tr("Docudrama");
    categoryDesc[0x2a] = QObject::tr("Documentary");
    categoryDesc[0x2b] = QObject::tr("Drama");
    categoryDesc[0x2c] = QObject::tr("Educational");
    categoryDesc[0x2f] = QObject::tr("Excercise");
    categoryDesc[0x31] = QObject::tr("Fantasy");
    categoryDesc[0x32] = QObject::tr("Fasion");
    categoryDesc[0x34] = QObject::tr("Fishing");
    categoryDesc[0x35] = QObject::tr("Football");
    categoryDesc[0x36] = QObject::tr("French");
    categoryDesc[0x37] = QObject::tr("Fundraiser");
    categoryDesc[0x38] = QObject::tr("Game show");
    categoryDesc[0x39] = QObject::tr("Golf");
    categoryDesc[0x3a] = QObject::tr("Gymnastics");
    categoryDesc[0x3b] = QObject::tr("Health");
    categoryDesc[0x3c] = QObject::tr("History");
    categoryDesc[0x3d] = QObject::tr("Historical drama");
    categoryDesc[0x3e] = QObject::tr("Hockey");
    categoryDesc[0x3f] = QObject::tr("Holiday");
    categoryDesc[0x40] = QObject::tr("Holiday-children");
    categoryDesc[0x41] = QObject::tr("Holiday-children special");
    categoryDesc[0x44] = QObject::tr("Holiday special");
    categoryDesc[0x45] = QObject::tr("Horror");
    categoryDesc[0x46] = QObject::tr("Horse racing");
    categoryDesc[0x47] = QObject::tr("House/garden");
    categoryDesc[0x49] = QObject::tr("How-to");
    categoryDesc[0x4b] = QObject::tr("Interview");
    categoryDesc[0x4d] = QObject::tr("Lacrosse");
    categoryDesc[0x4f] = QObject::tr("Martial Arts");
    categoryDesc[0x50] = QObject::tr("Medical");
    categoryDesc[0x51] = QObject::tr("Miniseries");
    categoryDesc[0x52] = QObject::tr("Motorsports");
    categoryDesc[0x53] = QObject::tr("Motorcycle");
    categoryDesc[0x54] = QObject::tr("Music");
    categoryDesc[0x55] = QObject::tr("Music special");
    categoryDesc[0x56] = QObject::tr("Music talk");
    categoryDesc[0x57] = QObject::tr("Musical");
    categoryDesc[0x58] = QObject::tr("Musical comedy");
    categoryDesc[0x5a] = QObject::tr("Mystery");
    categoryDesc[0x5b] = QObject::tr("Nature");
    categoryDesc[0x5c] = QObject::tr("News");
    categoryDesc[0x5f] = QObject::tr("Opera");
    categoryDesc[0x60] = QObject::tr("Outdoors");
    categoryDesc[0x63] = QObject::tr("Public affairs");
    categoryDesc[0x66] = QObject::tr("Reality");
    categoryDesc[0x67] = QObject::tr("Religious");
    categoryDesc[0x68] = QObject::tr("Rodeo");
    categoryDesc[0x69] = QObject::tr("Romance");
    categoryDesc[0x6a] = QObject::tr("Romance-comedy");
    categoryDesc[0x6b] = QObject::tr("Rugby");
    categoryDesc[0x6c] = QObject::tr("Running");
    categoryDesc[0x6e] = QObject::tr("Science");
    categoryDesc[0x6f] = QObject::tr("Science fiction");
    categoryDesc[0x70] = QObject::tr("Self improvement");
    categoryDesc[0x71] = QObject::tr("Shopping");
    categoryDesc[0x74] = QObject::tr("Skiing");
    categoryDesc[0x77] = QObject::tr("Soap");
    categoryDesc[0x7b] = QObject::tr("Soccor");
    categoryDesc[0x7c] = QObject::tr("Softball");
    categoryDesc[0x7d] = QObject::tr("Spanish");
    categoryDesc[0x7e] = QObject::tr("Special");
    categoryDesc[0x81] = QObject::tr("Sports non-event");
    categoryDesc[0x82] = QObject::tr("Sports talk");
    categoryDesc[0x83] = QObject::tr("Suspense");
    categoryDesc[0x85] = QObject::tr("Swimming");
    categoryDesc[0x86] = QObject::tr("Talk");
    categoryDesc[0x87] = QObject::tr("Tennis");
    categoryDesc[0x89] = QObject::tr("Track/field");
    categoryDesc[0x8a] = QObject::tr("Travel");
    categoryDesc[0x8b] = QObject::tr("Variety");
    categoryDesc[0x8c] = QObject::tr("Volleyball");
    categoryDesc[0x8d] = QObject::tr("War");
    categoryDesc[0x8e] = QObject::tr("Watersports");
    categoryDesc[0x8f] = QObject::tr("Weather");
    categoryDesc[0x90] = QObject::tr("Western");
    categoryDesc[0x92] = QObject::tr("Wrestling");
    categoryDesc[0x93] = QObject::tr("Yoga");
    categoryDesc[0x94] = QObject::tr("Agriculture");
    categoryDesc[0x95] = QObject::tr("Anime");
    categoryDesc[0x97] = QObject::tr("Arm Wrestling");
    categoryDesc[0x98] = QObject::tr("Arts/crafts");
    categoryDesc[0x99] = QObject::tr("Auction");
    categoryDesc[0x9a] = QObject::tr("Auto racing");
    categoryDesc[0x9b] = QObject::tr("Air racing");
    categoryDesc[0x9c] = QObject::tr("Badminton");
    categoryDesc[0xa0] = QObject::tr("Bicycle racing");
    categoryDesc[0xa1] = QObject::tr("Boat Racing");
    categoryDesc[0xa6] = QObject::tr("Community");
    categoryDesc[0xa7] = QObject::tr("Consumer");
    categoryDesc[0xaa] = QObject::tr("Debate");
    categoryDesc[0xac] = QObject::tr("Dog show");
    categoryDesc[0xad] = QObject::tr("Drag racing");
    categoryDesc[0xae] = QObject::tr("Entertainment");
    categoryDesc[0xaf] = QObject::tr("Environment");
    categoryDesc[0xb0] = QObject::tr("Equestrian");
    categoryDesc[0xb3] = QObject::tr("Field hockey");
    categoryDesc[0xb5] = QObject::tr("Football");
    categoryDesc[0xb6] = QObject::tr("Gay/lesbian");
    categoryDesc[0xb7] = QObject::tr("Handball");
    categoryDesc[0xb8] = QObject::tr("Home improvement");
    categoryDesc[0xb9] = QObject::tr("Hunting");
    categoryDesc[0xbb] = QObject::tr("Hydroplane racing");
    categoryDesc[0xc1] = QObject::tr("Law");
    categoryDesc[0xc3] = QObject::tr("Motorcycle racing");
    categoryDesc[0xc5] = QObject::tr("Newsmagazine");
    categoryDesc[0xc7] = QObject::tr("Paranormal");
    categoryDesc[0xc8] = QObject::tr("Parenting");
    categoryDesc[0xca] = QObject::tr("Performing arts");
    categoryDesc[0xcc] = QObject::tr("Politics");
    categoryDesc[0xcf] = QObject::tr("Pro wrestling");
    categoryDesc[0xd3] = QObject::tr("Sailing");
    categoryDesc[0xd4] = QObject::tr("Shooting");
    categoryDesc[0xd5] = QObject::tr("Sitcom");
    categoryDesc[0xd6] = QObject::tr("Skateboarding");
    categoryDesc[0xd9] = QObject::tr("Snowboarding");
    categoryDesc[0xdd] = QObject::tr("Standup");
    categoryDesc[0xdf] = QObject::tr("Surfing");
    categoryDesc[0xe0] = QObject::tr("Tennis");
    categoryDesc[0xe1] = QObject::tr("Triathlon");
    categoryDesc[0xe6] = QObject::tr("Card games");
    categoryDesc[0xe7] = QObject::tr("Poker");
    categoryDesc[0xea] = QObject::tr("Military");
    categoryDesc[0xeb] = QObject::tr("Technology");
    categoryDesc[0xec] = QObject::tr("Mixed martial arts");
    categoryDesc[0xed] = QObject::tr("Action sports");
    categoryDesc[0xff] = QObject::tr("Dish Network");

    categoriesExists = true;
}


