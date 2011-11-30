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

QMutex             DishEventMPAADescriptor::mpaaRatingsLock;
QMap<uint,QString> DishEventMPAADescriptor::mpaaRatingsDesc;
bool               DishEventMPAADescriptor::mpaaRatingsExists = false;

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

    QMap<uint,QString>::const_iterator it = mpaaRatingsDesc.find(rating_raw());
    if (it != mpaaRatingsDesc.end())
        return *it;

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

QMutex             DishEventVCHIPDescriptor::vchipRatingsLock;
QMap<uint,QString> DishEventVCHIPDescriptor::vchipRatingsDesc;
bool               DishEventVCHIPDescriptor::vchipRatingsExists = false;

QString DishEventVCHIPDescriptor::rating(void) const
{
    if (!vchipRatingsExists)
        Init();

    QMutexLocker locker(&vchipRatingsLock);

    QMap<uint,QString>::const_iterator it = vchipRatingsDesc.find(rating_raw());
    if (it != vchipRatingsDesc.end())
        return *it;

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

QMap<uint,QString> DishContentDescriptor::themeDesc;
QMap<uint,QString> DishContentDescriptor::dishCategoryDesc;
volatile bool      DishContentDescriptor::dishCategoryDescExists = false;

QString dish_theme_type_to_string(uint theme_type)
{
    static const char *themes[kThemeLast] =
    {
        "", "Movie", "Sports", "News/Business", "Family/Children", "Education",
        "Series/Special", "Music/Art", "Religious", "Off-Air"
    };

    if ((theme_type > kThemeNone) && (theme_type < kThemeLast))
        return QString(themes[theme_type]);

    return "";
}

DishThemeType string_to_dish_theme_type(const QString &theme_type)
{
    static const char *themes[kThemeLast] =
    {
        "", "Movie", "Sports", "News/Business", "Family/Children", "Education",
        "Series/Special", "Music/Art", "Religious", "Off-Air"
    };

    for (uint i = 1; i < 10; i++)
        if (theme_type == themes[i])
            return (DishThemeType) i;

    return kThemeNone;
}

DishThemeType DishContentDescriptor::GetTheme(void) const
{
    if (!dishCategoryDescExists)
        Init();

    if (Nibble1(0) == 0x00)
        return kThemeOffAir;

    QMap<uint,QString>::const_iterator it = themeDesc.find(Nibble2(0));
    if (it != themeDesc.end())
        return string_to_dish_theme_type(*it);

    // Found nothing? Just return empty string.
    return kThemeNone;
}

QString DishContentDescriptor::GetCategory(void) const
{
    if (!dishCategoryDescExists)
        Init();

    QMutexLocker locker(&categoryLock);

    // Try to get detailed description
    QMap<uint,QString>::const_iterator it =
        dishCategoryDesc.find(UserNibble(0));
    if (it != dishCategoryDesc.end())
        return *it;

    // Fallback to just the theme
    QString theme = dish_theme_type_to_string(GetTheme());

    return theme;
}

QString DishContentDescriptor::toString() const
{
    return QString("%1 : %2").arg(int(GetTheme())).arg(GetCategory());
}

void DishContentDescriptor::Init(void)
{
    ContentDescriptor::Init();

    QMutexLocker locker(&categoryLock);

    if (dishCategoryDescExists)
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
    dishCategoryDesc[0x01] = "Action";
    dishCategoryDesc[0x02] = "Adults only";
    dishCategoryDesc[0x03] = "Adventure";
    dishCategoryDesc[0x04] = "Animals";
    dishCategoryDesc[0x05] = "Animated";
    dishCategoryDesc[0x07] = "Anthology";
    dishCategoryDesc[0x08] = "Art";
    dishCategoryDesc[0x09] = "Auto";
    dishCategoryDesc[0x0a] = "Awards";
    dishCategoryDesc[0x0b] = "Ballet";
    dishCategoryDesc[0x0c] = "Baseball";
    dishCategoryDesc[0x0d] = "Basketball";
    dishCategoryDesc[0x11] = "Biography";
    dishCategoryDesc[0x12] = "Boat";
    dishCategoryDesc[0x14] = "Bowling";
    dishCategoryDesc[0x15] = "Boxing";
    dishCategoryDesc[0x16] = "Bus./financial";
    dishCategoryDesc[0x1a] = "Children";
    dishCategoryDesc[0x1b] = "Children-special";
    dishCategoryDesc[0x1c] = "Children-news";
    dishCategoryDesc[0x1d] = "Children-music";
    dishCategoryDesc[0x1f] = "Collectibles";
    dishCategoryDesc[0x20] = "Comedy";
    dishCategoryDesc[0x21] = "Comedy-drama";
    dishCategoryDesc[0x22] = "Computers";
    dishCategoryDesc[0x23] = "Cooking";
    dishCategoryDesc[0x24] = "Crime";
    dishCategoryDesc[0x25] = "Crime drama";
    dishCategoryDesc[0x27] = "Dance";
    dishCategoryDesc[0x29] = "Docudrama";
    dishCategoryDesc[0x2a] = "Documentary";
    dishCategoryDesc[0x2b] = "Drama";
    dishCategoryDesc[0x2c] = "Educational";
    dishCategoryDesc[0x2f] = "Excercise";
    dishCategoryDesc[0x31] = "Fantasy";
    dishCategoryDesc[0x32] = "Fasion";
    dishCategoryDesc[0x34] = "Fishing";
    dishCategoryDesc[0x35] = "Football";
    dishCategoryDesc[0x36] = "French";
    dishCategoryDesc[0x37] = "Fundraiser";
    dishCategoryDesc[0x38] = "Game show";
    dishCategoryDesc[0x39] = "Golf";
    dishCategoryDesc[0x3a] = "Gymnastics";
    dishCategoryDesc[0x3b] = "Health";
    dishCategoryDesc[0x3c] = "History";
    dishCategoryDesc[0x3d] = "Historical drama";
    dishCategoryDesc[0x3e] = "Hockey";
    dishCategoryDesc[0x3f] = "Holiday";
    dishCategoryDesc[0x40] = "Holiday-children";
    dishCategoryDesc[0x41] = "Holiday-children special";
    dishCategoryDesc[0x44] = "Holiday special";
    dishCategoryDesc[0x45] = "Horror";
    dishCategoryDesc[0x46] = "Horse racing";
    dishCategoryDesc[0x47] = "House/garden";
    dishCategoryDesc[0x49] = "How-to";
    dishCategoryDesc[0x4b] = "Interview";
    dishCategoryDesc[0x4d] = "Lacrosse";
    dishCategoryDesc[0x4f] = "Martial Arts";
    dishCategoryDesc[0x50] = "Medical";
    dishCategoryDesc[0x51] = "Miniseries";
    dishCategoryDesc[0x52] = "Motorsports";
    dishCategoryDesc[0x53] = "Motorcycle";
    dishCategoryDesc[0x54] = "Music";
    dishCategoryDesc[0x55] = "Music special";
    dishCategoryDesc[0x56] = "Music talk";
    dishCategoryDesc[0x57] = "Musical";
    dishCategoryDesc[0x58] = "Musical comedy";
    dishCategoryDesc[0x5a] = "Mystery";
    dishCategoryDesc[0x5b] = "Nature";
    dishCategoryDesc[0x5c] = "News";
    dishCategoryDesc[0x5f] = "Opera";
    dishCategoryDesc[0x60] = "Outdoors";
    dishCategoryDesc[0x63] = "Public affairs";
    dishCategoryDesc[0x66] = "Reality";
    dishCategoryDesc[0x67] = "Religious";
    dishCategoryDesc[0x68] = "Rodeo";
    dishCategoryDesc[0x69] = "Romance";
    dishCategoryDesc[0x6a] = "Romance-comedy";
    dishCategoryDesc[0x6b] = "Rugby";
    dishCategoryDesc[0x6c] = "Running";
    dishCategoryDesc[0x6e] = "Science";
    dishCategoryDesc[0x6f] = "Science fiction";
    dishCategoryDesc[0x70] = "Self improvement";
    dishCategoryDesc[0x71] = "Shopping";
    dishCategoryDesc[0x74] = "Skiing";
    dishCategoryDesc[0x77] = "Soap";
    dishCategoryDesc[0x7b] = "Soccor";
    dishCategoryDesc[0x7c] = "Softball";
    dishCategoryDesc[0x7d] = "Spanish";
    dishCategoryDesc[0x7e] = "Special";
    dishCategoryDesc[0x81] = "Sports non-event";
    dishCategoryDesc[0x82] = "Sports talk";
    dishCategoryDesc[0x83] = "Suspense";
    dishCategoryDesc[0x85] = "Swimming";
    dishCategoryDesc[0x86] = "Talk";
    dishCategoryDesc[0x87] = "Tennis";
    dishCategoryDesc[0x89] = "Track/field";
    dishCategoryDesc[0x8a] = "Travel";
    dishCategoryDesc[0x8b] = "Variety";
    dishCategoryDesc[0x8c] = "Volleyball";
    dishCategoryDesc[0x8d] = "War";
    dishCategoryDesc[0x8e] = "Watersports";
    dishCategoryDesc[0x8f] = "Weather";
    dishCategoryDesc[0x90] = "Western";
    dishCategoryDesc[0x92] = "Wrestling";
    dishCategoryDesc[0x93] = "Yoga";
    dishCategoryDesc[0x94] = "Agriculture";
    dishCategoryDesc[0x95] = "Anime";
    dishCategoryDesc[0x97] = "Arm Wrestling";
    dishCategoryDesc[0x98] = "Arts/crafts";
    dishCategoryDesc[0x99] = "Auction";
    dishCategoryDesc[0x9a] = "Auto racing";
    dishCategoryDesc[0x9b] = "Air racing";
    dishCategoryDesc[0x9c] = "Badminton";
    dishCategoryDesc[0xa0] = "Bicycle racing";
    dishCategoryDesc[0xa1] = "Boat Racing";
    dishCategoryDesc[0xa6] = "Community";
    dishCategoryDesc[0xa7] = "Consumer";
    dishCategoryDesc[0xaa] = "Debate";
    dishCategoryDesc[0xac] = "Dog show";
    dishCategoryDesc[0xad] = "Drag racing";
    dishCategoryDesc[0xae] = "Entertainment";
    dishCategoryDesc[0xaf] = "Environment";
    dishCategoryDesc[0xb0] = "Equestrian";
    dishCategoryDesc[0xb3] = "Field hockey";
    dishCategoryDesc[0xb5] = "Football";
    dishCategoryDesc[0xb6] = "Gay/lesbian";
    dishCategoryDesc[0xb7] = "Handball";
    dishCategoryDesc[0xb8] = "Home improvement";
    dishCategoryDesc[0xb9] = "Hunting";
    dishCategoryDesc[0xbb] = "Hydroplane racing";
    dishCategoryDesc[0xc1] = "Law";
    dishCategoryDesc[0xc3] = "Motorcycle racing";
    dishCategoryDesc[0xc5] = "Newsmagazine";
    dishCategoryDesc[0xc7] = "Paranormal";
    dishCategoryDesc[0xc8] = "Parenting";
    dishCategoryDesc[0xca] = "Performing arts";
    dishCategoryDesc[0xcc] = "Politics";
    dishCategoryDesc[0xcf] = "Pro wrestling";
    dishCategoryDesc[0xd3] = "Sailing";
    dishCategoryDesc[0xd4] = "Shooting";
    dishCategoryDesc[0xd5] = "Sitcom";
    dishCategoryDesc[0xd6] = "Skateboarding";
    dishCategoryDesc[0xd9] = "Snowboarding";
    dishCategoryDesc[0xdd] = "Standup";
    dishCategoryDesc[0xdf] = "Surfing";
    dishCategoryDesc[0xe0] = "Tennis";
    dishCategoryDesc[0xe1] = "Triathlon";
    dishCategoryDesc[0xe6] = "Card games";
    dishCategoryDesc[0xe7] = "Poker";
    dishCategoryDesc[0xea] = "Military";
    dishCategoryDesc[0xeb] = "Technology";
    dishCategoryDesc[0xec] = "Mixed martial arts";
    dishCategoryDesc[0xed] = "Action sports";
    dishCategoryDesc[0xff] = "Dish Network";

    dishCategoryDescExists = true;
}


