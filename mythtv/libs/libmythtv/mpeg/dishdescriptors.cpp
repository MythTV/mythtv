// -*- Mode: c++ -*-

// Qt headers
#include <QCoreApplication>

// MythTV headers
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
    themeDesc[kThemeMovie]     = QCoreApplication::translate("(Categories)",
                                                             "Movie");
    themeDesc[kThemeSports]    = QCoreApplication::translate("(Categories)",
                                                             "Sports");
    themeDesc[kThemeNews]      = QCoreApplication::translate("(Categories)",
                                                             "News/Business");
    themeDesc[kThemeFamily]    = QCoreApplication::translate("(Categories)",
                                                             "Family/Children");
    themeDesc[kThemeEducation] = QCoreApplication::translate("(Categories)",
                                                             "Education");
    themeDesc[kThemeSeries]    = QCoreApplication::translate("(Categories)",
                                                             "Series/Special");
    themeDesc[kThemeMusic]     = QCoreApplication::translate("(Categories)",
                                                             "Music/Art");
    themeDesc[kThemeReligious] = QCoreApplication::translate("(Categories)",
                                                             "Religious");

    // Dish/BEV categories
    dishCategoryDesc[0x01] = QCoreApplication::translate("(Categories)",
                                                         "Action");
    dishCategoryDesc[0x02] = QCoreApplication::translate("(Categories)",
                                                         "Adults only");
    dishCategoryDesc[0x03] = QCoreApplication::translate("(Categories)",
                                                         "Adventure");
    dishCategoryDesc[0x04] = QCoreApplication::translate("(Categories)",
                                                         "Animals");
    dishCategoryDesc[0x05] = QCoreApplication::translate("(Categories)",
                                                         "Animated");
    dishCategoryDesc[0x07] = QCoreApplication::translate("(Categories)",
                                                         "Anthology");
    dishCategoryDesc[0x08] = QCoreApplication::translate("(Categories)",
                                                         "Art");
    dishCategoryDesc[0x09] = QCoreApplication::translate("(Categories)",
                                                         "Auto");
    dishCategoryDesc[0x0a] = QCoreApplication::translate("(Categories)",
                                                         "Awards");
    dishCategoryDesc[0x0b] = QCoreApplication::translate("(Categories)",
                                                         "Ballet");
    dishCategoryDesc[0x0c] = QCoreApplication::translate("(Categories)",
                                                         "Baseball");
    dishCategoryDesc[0x0d] = QCoreApplication::translate("(Categories)",
                                                         "Basketball");
    dishCategoryDesc[0x11] = QCoreApplication::translate("(Categories)",
                                                         "Biography");
    dishCategoryDesc[0x12] = QCoreApplication::translate("(Categories)",
                                                         "Boat");
    dishCategoryDesc[0x14] = QCoreApplication::translate("(Categories)",
                                                         "Bowling");
    dishCategoryDesc[0x15] = QCoreApplication::translate("(Categories)",
                                                         "Boxing");
    dishCategoryDesc[0x16] = QCoreApplication::translate("(Categories)",
                                                         "Bus./financial");
    dishCategoryDesc[0x1a] = QCoreApplication::translate("(Categories)",
                                                         "Children");
    dishCategoryDesc[0x1b] = QCoreApplication::translate("(Categories)",
                                                         "Children-special");
    dishCategoryDesc[0x1c] = QCoreApplication::translate("(Categories)",
                                                         "Children-news");
    dishCategoryDesc[0x1d] = QCoreApplication::translate("(Categories)",
                                                         "Children-music");
    dishCategoryDesc[0x1f] = QCoreApplication::translate("(Categories)",
                                                         "Collectibles");
    dishCategoryDesc[0x20] = QCoreApplication::translate("(Categories)",
                                                         "Comedy");
    dishCategoryDesc[0x21] = QCoreApplication::translate("(Categories)",
                                                         "Comedy-drama");
    dishCategoryDesc[0x22] = QCoreApplication::translate("(Categories)",
                                                         "Computers");
    dishCategoryDesc[0x23] = QCoreApplication::translate("(Categories)",
                                                         "Cooking");
    dishCategoryDesc[0x24] = QCoreApplication::translate("(Categories)",
                                                         "Crime");
    dishCategoryDesc[0x25] = QCoreApplication::translate("(Categories)",
                                                         "Crime drama");
    dishCategoryDesc[0x27] = QCoreApplication::translate("(Categories)",
                                                         "Dance");
    dishCategoryDesc[0x29] = QCoreApplication::translate("(Categories)",
                                                         "Docudrama");
    dishCategoryDesc[0x2a] = QCoreApplication::translate("(Categories)",
                                                         "Documentary");
    dishCategoryDesc[0x2b] = QCoreApplication::translate("(Categories)",
                                                         "Drama");
    dishCategoryDesc[0x2c] = QCoreApplication::translate("(Categories)",
                                                         "Educational");
    dishCategoryDesc[0x2f] = QCoreApplication::translate("(Categories)",
                                                         "Exercise");
    dishCategoryDesc[0x31] = QCoreApplication::translate("(Categories)",
                                                         "Fantasy");
    dishCategoryDesc[0x32] = QCoreApplication::translate("(Categories)",
                                                         "Fashion");
    dishCategoryDesc[0x34] = QCoreApplication::translate("(Categories)",
                                                         "Fishing");
    dishCategoryDesc[0x35] = QCoreApplication::translate("(Categories)",
                                                         "Football");
    dishCategoryDesc[0x36] = QCoreApplication::translate("(Categories)",
                                                         "French");
    dishCategoryDesc[0x37] = QCoreApplication::translate("(Categories)",
                                                         "Fundraiser");
    dishCategoryDesc[0x38] = QCoreApplication::translate("(Categories)",
                                                         "Game show");
    dishCategoryDesc[0x39] = QCoreApplication::translate("(Categories)",
                                                         "Golf");
    dishCategoryDesc[0x3a] = QCoreApplication::translate("(Categories)",
                                                         "Gymnastics");
    dishCategoryDesc[0x3b] = QCoreApplication::translate("(Categories)",
                                                         "Health");
    dishCategoryDesc[0x3c] = QCoreApplication::translate("(Categories)",
                                                         "History");
    dishCategoryDesc[0x3d] = QCoreApplication::translate("(Categories)",
                                                         "Historical drama");
    dishCategoryDesc[0x3e] = QCoreApplication::translate("(Categories)",
                                                         "Hockey");
    dishCategoryDesc[0x3f] = QCoreApplication::translate("(Categories)",
                                                         "Holiday");
    dishCategoryDesc[0x40] = QCoreApplication::translate("(Categories)",
                                                         "Holiday-children");
    dishCategoryDesc[0x41] = QCoreApplication::translate("(Categories)",
                                "Holiday-children special");
    dishCategoryDesc[0x44] = QCoreApplication::translate("(Categories)",
                                                         "Holiday special");
    dishCategoryDesc[0x45] = QCoreApplication::translate("(Categories)",
                                                         "Horror");
    dishCategoryDesc[0x46] = QCoreApplication::translate("(Categories)",
                                                         "Horse racing");
    dishCategoryDesc[0x47] = QCoreApplication::translate("(Categories)",
                                                         "House/garden");
    dishCategoryDesc[0x49] = QCoreApplication::translate("(Categories)",
                                                         "How-to");
    dishCategoryDesc[0x4b] = QCoreApplication::translate("(Categories)",
                                                         "Interview");
    dishCategoryDesc[0x4d] = QCoreApplication::translate("(Categories)",
                                                         "Lacrosse");
    dishCategoryDesc[0x4f] = QCoreApplication::translate("(Categories)",
                                                         "Martial Arts");
    dishCategoryDesc[0x50] = QCoreApplication::translate("(Categories)",
                                                         "Medical");
    dishCategoryDesc[0x51] = QCoreApplication::translate("(Categories)",
                                                         "Miniseries");
    dishCategoryDesc[0x52] = QCoreApplication::translate("(Categories)",
                                                         "Motorsports");
    dishCategoryDesc[0x53] = QCoreApplication::translate("(Categories)",
                                                         "Motorcycle");
    dishCategoryDesc[0x54] = QCoreApplication::translate("(Categories)",
                                                         "Music");
    dishCategoryDesc[0x55] = QCoreApplication::translate("(Categories)",
                                                         "Music special");
    dishCategoryDesc[0x56] = QCoreApplication::translate("(Categories)",
                                                         "Music talk");
    dishCategoryDesc[0x57] = QCoreApplication::translate("(Categories)",
                                                         "Musical");
    dishCategoryDesc[0x58] = QCoreApplication::translate("(Categories)",
                                                         "Musical comedy");
    dishCategoryDesc[0x5a] = QCoreApplication::translate("(Categories)",
                                                         "Mystery");
    dishCategoryDesc[0x5b] = QCoreApplication::translate("(Categories)",
                                                         "Nature");
    dishCategoryDesc[0x5c] = QCoreApplication::translate("(Categories)",
                                                         "News");
    dishCategoryDesc[0x5f] = QCoreApplication::translate("(Categories)",
                                                         "Opera");
    dishCategoryDesc[0x60] = QCoreApplication::translate("(Categories)",
                                                         "Outdoors");
    dishCategoryDesc[0x63] = QCoreApplication::translate("(Categories)",
                                                         "Public affairs");
    dishCategoryDesc[0x66] = QCoreApplication::translate("(Categories)",
                                                         "Reality");
    dishCategoryDesc[0x67] = QCoreApplication::translate("(Categories)",
                                                         "Religious");
    dishCategoryDesc[0x68] = QCoreApplication::translate("(Categories)",
                                                         "Rodeo");
    dishCategoryDesc[0x69] = QCoreApplication::translate("(Categories)",
                                                         "Romance");
    dishCategoryDesc[0x6a] = QCoreApplication::translate("(Categories)",
                                                         "Romance-comedy");
    dishCategoryDesc[0x6b] = QCoreApplication::translate("(Categories)",
                                                         "Rugby");
    dishCategoryDesc[0x6c] = QCoreApplication::translate("(Categories)",
                                                         "Running");
    dishCategoryDesc[0x6e] = QCoreApplication::translate("(Categories)",
                                                         "Science");
    dishCategoryDesc[0x6f] = QCoreApplication::translate("(Categories)",
                                                         "Science fiction");
    dishCategoryDesc[0x70] = QCoreApplication::translate("(Categories)",
                                                         "Self improvement");
    dishCategoryDesc[0x71] = QCoreApplication::translate("(Categories)",
                                                         "Shopping");
    dishCategoryDesc[0x74] = QCoreApplication::translate("(Categories)",
                                                         "Skiing");
    dishCategoryDesc[0x77] = QCoreApplication::translate("(Categories)",
                                                         "Soap");
    dishCategoryDesc[0x7b] = QCoreApplication::translate("(Categories)",
                                                         "Soccer");
    dishCategoryDesc[0x7c] = QCoreApplication::translate("(Categories)",
                                                         "Softball");
    dishCategoryDesc[0x7d] = QCoreApplication::translate("(Categories)",
                                                         "Spanish");
    dishCategoryDesc[0x7e] = QCoreApplication::translate("(Categories)",
                                                         "Special");
    dishCategoryDesc[0x81] = QCoreApplication::translate("(Categories)",
                                                         "Sports non-event");
    dishCategoryDesc[0x82] = QCoreApplication::translate("(Categories)",
                                                         "Sports talk");
    dishCategoryDesc[0x83] = QCoreApplication::translate("(Categories)",
                                                         "Suspense");
    dishCategoryDesc[0x85] = QCoreApplication::translate("(Categories)",
                                                         "Swimming");
    dishCategoryDesc[0x86] = QCoreApplication::translate("(Categories)",
                                                         "Talk");
    dishCategoryDesc[0x87] = QCoreApplication::translate("(Categories)",
                                                         "Tennis");
    dishCategoryDesc[0x89] = QCoreApplication::translate("(Categories)",
                                                         "Track/field");
    dishCategoryDesc[0x8a] = QCoreApplication::translate("(Categories)",
                                                         "Travel");
    dishCategoryDesc[0x8b] = QCoreApplication::translate("(Categories)",
                                                         "Variety");
    dishCategoryDesc[0x8c] = QCoreApplication::translate("(Categories)",
                                                         "Volleyball");
    dishCategoryDesc[0x8d] = QCoreApplication::translate("(Categories)",
                                                         "War");
    dishCategoryDesc[0x8e] = QCoreApplication::translate("(Categories)",
                                                         "Watersports");
    dishCategoryDesc[0x8f] = QCoreApplication::translate("(Categories)",
                                                         "Weather");
    dishCategoryDesc[0x90] = QCoreApplication::translate("(Categories)",
                                                         "Western");
    dishCategoryDesc[0x92] = QCoreApplication::translate("(Categories)",
                                                         "Wrestling");
    dishCategoryDesc[0x93] = QCoreApplication::translate("(Categories)",
                                                         "Yoga");
    dishCategoryDesc[0x94] = QCoreApplication::translate("(Categories)",
                                                         "Agriculture");
    dishCategoryDesc[0x95] = QCoreApplication::translate("(Categories)",
                                                         "Anime");
    dishCategoryDesc[0x97] = QCoreApplication::translate("(Categories)",
                                                         "Arm Wrestling");
    dishCategoryDesc[0x98] = QCoreApplication::translate("(Categories)",
                                                         "Arts/crafts");
    dishCategoryDesc[0x99] = QCoreApplication::translate("(Categories)",
                                                         "Auction");
    dishCategoryDesc[0x9a] = QCoreApplication::translate("(Categories)",
                                                         "Auto racing");
    dishCategoryDesc[0x9b] = QCoreApplication::translate("(Categories)",
                                                         "Air racing");
    dishCategoryDesc[0x9c] = QCoreApplication::translate("(Categories)",
                                                         "Badminton");
    dishCategoryDesc[0xa0] = QCoreApplication::translate("(Categories)",
                                                         "Bicycle racing");
    dishCategoryDesc[0xa1] = QCoreApplication::translate("(Categories)",
                                                         "Boat Racing");
    dishCategoryDesc[0xa6] = QCoreApplication::translate("(Categories)",
                                                         "Community");
    dishCategoryDesc[0xa7] = QCoreApplication::translate("(Categories)",
                                                         "Consumer");
    dishCategoryDesc[0xaa] = QCoreApplication::translate("(Categories)",
                                                         "Debate");
    dishCategoryDesc[0xac] = QCoreApplication::translate("(Categories)",
                                                         "Dog show");
    dishCategoryDesc[0xad] = QCoreApplication::translate("(Categories)",
                                                         "Drag racing");
    dishCategoryDesc[0xae] = QCoreApplication::translate("(Categories)",
                                                         "Entertainment");
    dishCategoryDesc[0xaf] = QCoreApplication::translate("(Categories)",
                                                         "Environment");
    dishCategoryDesc[0xb0] = QCoreApplication::translate("(Categories)",
                                                         "Equestrian");
    dishCategoryDesc[0xb3] = QCoreApplication::translate("(Categories)",
                                                         "Field hockey");
    dishCategoryDesc[0xb5] = QCoreApplication::translate("(Categories)",
                                                         "Football");
    dishCategoryDesc[0xb6] = QCoreApplication::translate("(Categories)",
                                                         "Gay/lesbian");
    dishCategoryDesc[0xb7] = QCoreApplication::translate("(Categories)",
                                                         "Handball");
    dishCategoryDesc[0xb8] = QCoreApplication::translate("(Categories)",
                                                         "Home improvement");
    dishCategoryDesc[0xb9] = QCoreApplication::translate("(Categories)",
                                                         "Hunting");
    dishCategoryDesc[0xbb] = QCoreApplication::translate("(Categories)",
                                                         "Hydroplane racing");
    dishCategoryDesc[0xc1] = QCoreApplication::translate("(Categories)",
                                                         "Law");
    dishCategoryDesc[0xc3] = QCoreApplication::translate("(Categories)",
                                                         "Motorcycle racing");
    dishCategoryDesc[0xc5] = QCoreApplication::translate("(Categories)",
                                                         "Newsmagazine");
    dishCategoryDesc[0xc7] = QCoreApplication::translate("(Categories)",
                                                         "Paranormal");
    dishCategoryDesc[0xc8] = QCoreApplication::translate("(Categories)",
                                                         "Parenting");
    dishCategoryDesc[0xca] = QCoreApplication::translate("(Categories)",
                                                         "Performing arts");
    dishCategoryDesc[0xcc] = QCoreApplication::translate("(Categories)",
                                                         "Politics");
    dishCategoryDesc[0xcf] = QCoreApplication::translate("(Categories)",
                                                         "Pro wrestling");
    dishCategoryDesc[0xd3] = QCoreApplication::translate("(Categories)",
                                                         "Sailing");
    dishCategoryDesc[0xd4] = QCoreApplication::translate("(Categories)",
                                                         "Shooting");
    dishCategoryDesc[0xd5] = QCoreApplication::translate("(Categories)",
                                                         "Sitcom");
    dishCategoryDesc[0xd6] = QCoreApplication::translate("(Categories)",
                                                         "Skateboarding");
    dishCategoryDesc[0xd9] = QCoreApplication::translate("(Categories)",
                                                         "Snowboarding");
    dishCategoryDesc[0xdd] = QCoreApplication::translate("(Categories)",
                                                         "Standup");
    dishCategoryDesc[0xdf] = QCoreApplication::translate("(Categories)",
                                                         "Surfing");
    dishCategoryDesc[0xe0] = QCoreApplication::translate("(Categories)",
                                                         "Tennis");
    dishCategoryDesc[0xe1] = QCoreApplication::translate("(Categories)",
                                                         "Triathlon");
    dishCategoryDesc[0xe6] = QCoreApplication::translate("(Categories)",
                                                         "Card games");
    dishCategoryDesc[0xe7] = QCoreApplication::translate("(Categories)",
                                                         "Poker");
    dishCategoryDesc[0xea] = QCoreApplication::translate("(Categories)",
                                                         "Military");
    dishCategoryDesc[0xeb] = QCoreApplication::translate("(Categories)",
                                                         "Technology");
    dishCategoryDesc[0xec] = QCoreApplication::translate("(Categories)",
                                                         "Mixed martial arts");
    dishCategoryDesc[0xed] = QCoreApplication::translate("(Categories)",
                                                         "Action sports");
    dishCategoryDesc[0xff] = QCoreApplication::translate("(Categories)",
                                                         "Dish Network");

    dishCategoryDescExists = true;
}
