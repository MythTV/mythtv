// -*- Mode: c++ -*-

#include "dishdescriptors.h"
#include "atsc_huffman.h"
#include "programinfo.h" // for subtitle types and audio and video properties
#include "dvbtables.h"

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
    QString prefix = QString("");

    if (DescriptorLength() != 8)
        return QString::null;

    QString series = seriesid();
    series.remove(0, 2);

    uint episode = ((_data[6] & 0x3f) << 0x08) | _data[7];

    if (_data[2] == 0x7c)
        prefix = "MV";
    else if (_data[2] == 0x7d)
        prefix = "SP";
    else if (_data[2] == 0x7e)
    {
        if (episode > 0)
            prefix = "EP";
        else
            prefix = "SH";
    } else
        return prefix;

    QString id = QString("%1%2%3").arg(prefix).arg(series).arg(episode, 4, 0);

    return id;      
}

QString DishEventTagsDescriptor::seriesid(void) const
{
    QString prefix = QString("");

    if (DescriptorLength() != 8)
        return QString::null;

    if (_data[2] == 0x7c)
        prefix = "MV";
    else if (_data[2] == 0x7d)
        prefix = "SP";
    else if (_data[2] == 0x7e)
        prefix = "EP";
    else
        return prefix;

    uint series  = (_data[3] << 0x12) | (_data[4] << 0x0a) | (_data[5] << 0x02) |
                    ((_data[6] & 0xc0) >> 0x06);

    QString id = QString("%1%2").arg(prefix).arg(series, 8, 0);

    return id;
}

QDate DishEventTagsDescriptor::originalairdate(void) const
{
    unsigned char mjd[5];

    if (DescriptorLength() != 8)
        return QDate();

    mjd[0] = _data[8];
    mjd[1] = _data[9];
    mjd[2] = 0;
    mjd[3] = 0;
    mjd[4] = 0;

    QDateTime t = dvbdate2qt(mjd);

    if (!t.isValid())
        return QDate();

    QDate originalairdate = t.date();

    if (originalairdate.year() < 1940)
        return QDate();

    return originalairdate;
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

    mpaaRatingsDesc[0x01] = "G";
    mpaaRatingsDesc[0x02] = "PG";
    mpaaRatingsDesc[0x03] = "PG-13";
    mpaaRatingsDesc[0x04] = "R";
    mpaaRatingsDesc[0x05] = "NC-17";
    mpaaRatingsDesc[0x06] = "NR";

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

    vchipRatingsDesc[0x01] = "TV-Y";
    vchipRatingsDesc[0x02] = "TV-Y7";
    vchipRatingsDesc[0x03] = "TV-G";
    vchipRatingsDesc[0x04] = "TV-PG";
    vchipRatingsDesc[0x05] = "TV-14";
    vchipRatingsDesc[0x06] = "TV-MA";

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
    themeDesc[kThemeMovie]     = "Movie";
    themeDesc[kThemeSports]    = "Sports";
    themeDesc[kThemeNews]      = "News/Business";
    themeDesc[kThemeFamily]    = "Family/Children";
    themeDesc[kThemeEducation] = "Education";
    themeDesc[kThemeSeries]    = "Series/Special";
    themeDesc[kThemeMusic]     = "Music/Art";
    themeDesc[kThemeReligious] = "Religious";

    // Dish/BEV categories
    categoryDesc[0x01] = "Action";
    categoryDesc[0x02] = "Adults only";
    categoryDesc[0x03] = "Adventure";
    categoryDesc[0x04] = "Animals";
    categoryDesc[0x05] = "Animated";
    categoryDesc[0x07] = "Anthology";
    categoryDesc[0x08] = "Art";
    categoryDesc[0x09] = "Auto";
    categoryDesc[0x0a] = "Awards";
    categoryDesc[0x0b] = "Ballet";
    categoryDesc[0x0c] = "Baseball";
    categoryDesc[0x0d] = "Basketball";
    categoryDesc[0x11] = "Biography";
    categoryDesc[0x12] = "Boat";
    categoryDesc[0x14] = "Bowling";
    categoryDesc[0x15] = "Boxing";
    categoryDesc[0x16] = "Bus./financial";
    categoryDesc[0x1a] = "Children";
    categoryDesc[0x1b] = "Children-special";
    categoryDesc[0x1c] = "Children-news";
    categoryDesc[0x1d] = "Children-music";
    categoryDesc[0x1f] = "Collectibles";
    categoryDesc[0x20] = "Comedy";
    categoryDesc[0x21] = "Comedy-drama";
    categoryDesc[0x22] = "Computers";
    categoryDesc[0x23] = "Cooking";
    categoryDesc[0x24] = "Crime";
    categoryDesc[0x25] = "Crime drama";
    categoryDesc[0x27] = "Dance";
    categoryDesc[0x29] = "Docudrama";
    categoryDesc[0x2a] = "Documentary";
    categoryDesc[0x2b] = "Drama";
    categoryDesc[0x2c] = "Educational";
    categoryDesc[0x2f] = "Excercise";
    categoryDesc[0x31] = "Fantasy";
    categoryDesc[0x32] = "Fasion";
    categoryDesc[0x34] = "Fishing";
    categoryDesc[0x35] = "Football";
    categoryDesc[0x36] = "French";
    categoryDesc[0x37] = "Fundraiser";
    categoryDesc[0x38] = "Game show";
    categoryDesc[0x39] = "Golf";
    categoryDesc[0x3a] = "Gymnastics";
    categoryDesc[0x3b] = "Health";
    categoryDesc[0x3c] = "History";
    categoryDesc[0x3d] = "Historical drama";
    categoryDesc[0x3e] = "Hockey";
    categoryDesc[0x3f] = "Holiday";
    categoryDesc[0x40] = "Holiday-children";
    categoryDesc[0x41] = "Holiday-children special";
    categoryDesc[0x44] = "Holiday special";
    categoryDesc[0x45] = "Horror";
    categoryDesc[0x46] = "Horse racing";
    categoryDesc[0x47] = "House/garden";
    categoryDesc[0x49] = "How-to";
    categoryDesc[0x4b] = "Interview";
    categoryDesc[0x4d] = "Lacrosse";
    categoryDesc[0x4f] = "Martial Arts";
    categoryDesc[0x50] = "Medical";
    categoryDesc[0x51] = "Miniseries";
    categoryDesc[0x52] = "Motorsports";
    categoryDesc[0x53] = "Motorcycle";
    categoryDesc[0x54] = "Music";
    categoryDesc[0x55] = "Music special";
    categoryDesc[0x56] = "Music talk";
    categoryDesc[0x57] = "Musical";
    categoryDesc[0x58] = "Musical comedy";
    categoryDesc[0x5a] = "Mystery";
    categoryDesc[0x5b] = "Nature";
    categoryDesc[0x5c] = "News";
    categoryDesc[0x5f] = "Opera";
    categoryDesc[0x60] = "Outdoors";
    categoryDesc[0x63] = "Public affairs";
    categoryDesc[0x66] = "Reality";
    categoryDesc[0x67] = "Religious";
    categoryDesc[0x68] = "Rodeo";
    categoryDesc[0x69] = "Romance";
    categoryDesc[0x6a] = "Romance-comedy";
    categoryDesc[0x6b] = "Rugby";
    categoryDesc[0x6c] = "Running";
    categoryDesc[0x6e] = "Science";
    categoryDesc[0x6f] = "Science fiction";
    categoryDesc[0x70] = "Self improvement";
    categoryDesc[0x71] = "Shopping";
    categoryDesc[0x74] = "Skiing";
    categoryDesc[0x77] = "Soap";
    categoryDesc[0x7b] = "Soccor";
    categoryDesc[0x7c] = "Softball";
    categoryDesc[0x7d] = "Spanish";
    categoryDesc[0x7e] = "Special";
    categoryDesc[0x81] = "Sports non-event";
    categoryDesc[0x82] = "Sports talk";
    categoryDesc[0x83] = "Suspense";
    categoryDesc[0x85] = "Swimming";
    categoryDesc[0x86] = "Talk";
    categoryDesc[0x87] = "Tennis";
    categoryDesc[0x89] = "Track/field";
    categoryDesc[0x8a] = "Travel";
    categoryDesc[0x8b] = "Variety";
    categoryDesc[0x8c] = "Volleyball";
    categoryDesc[0x8d] = "War";
    categoryDesc[0x8e] = "Watersports";
    categoryDesc[0x8f] = "Weather";
    categoryDesc[0x90] = "Western";
    categoryDesc[0x92] = "Wrestling";
    categoryDesc[0x93] = "Yoga";
    categoryDesc[0x94] = "Agriculture";
    categoryDesc[0x95] = "Anime";
    categoryDesc[0x97] = "Arm Wrestling";
    categoryDesc[0x98] = "Arts/crafts";
    categoryDesc[0x99] = "Auction";
    categoryDesc[0x9a] = "Auto racing";
    categoryDesc[0x9b] = "Air racing";
    categoryDesc[0x9c] = "Badminton";
    categoryDesc[0xa0] = "Bicycle racing";
    categoryDesc[0xa1] = "Boat Racing";
    categoryDesc[0xa6] = "Community";
    categoryDesc[0xa7] = "Consumer";
    categoryDesc[0xaa] = "Debate";
    categoryDesc[0xac] = "Dog show";
    categoryDesc[0xad] = "Drag racing";
    categoryDesc[0xae] = "Entertainment";
    categoryDesc[0xaf] = "Environment";
    categoryDesc[0xb0] = "Equestrian";
    categoryDesc[0xb3] = "Field hockey";
    categoryDesc[0xb5] = "Football";
    categoryDesc[0xb6] = "Gay/lesbian";
    categoryDesc[0xb7] = "Handball";
    categoryDesc[0xb8] = "Home improvement";
    categoryDesc[0xb9] = "Hunting";
    categoryDesc[0xbb] = "Hydroplane racing";
    categoryDesc[0xc1] = "Law";
    categoryDesc[0xc3] = "Motorcycle racing";
    categoryDesc[0xc5] = "Newsmagazine";
    categoryDesc[0xc7] = "Paranormal";
    categoryDesc[0xc8] = "Parenting";
    categoryDesc[0xca] = "Performing arts";
    categoryDesc[0xcc] = "Politics";
    categoryDesc[0xcf] = "Pro wrestling";
    categoryDesc[0xd3] = "Sailing";
    categoryDesc[0xd4] = "Shooting";
    categoryDesc[0xd5] = "Sitcom";
    categoryDesc[0xd6] = "Skateboarding";
    categoryDesc[0xd9] = "Snowboarding";
    categoryDesc[0xdd] = "Standup";
    categoryDesc[0xdf] = "Surfing";
    categoryDesc[0xe0] = "Tennis";
    categoryDesc[0xe1] = "Triathlon";
    categoryDesc[0xe6] = "Card games";
    categoryDesc[0xe7] = "Poker";
    categoryDesc[0xea] = "Military";
    categoryDesc[0xeb] = "Technology";
    categoryDesc[0xec] = "Mixed martial arts";
    categoryDesc[0xed] = "Action sports";
    categoryDesc[0xff] = "Dish Network";

    categoriesExists = true;
}


