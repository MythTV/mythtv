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
        return QString();

    return atsc_huffman2_to_string(
        m_data + 3, DescriptorLength() - 1, compression_type);
}

const unsigned char *DishEventDescriptionDescriptor::DescriptionRaw(void) const
{
    if (DescriptorLength() <= 2)
        return nullptr;

    bool offset = (m_data[3] & 0xf8) == 0x80;
    return m_data + ((offset) ? 4 : 3);
}

uint DishEventDescriptionDescriptor::DescriptionRawLength(void) const
{
    if (DescriptorLength() <= 2)
        return 0;

    bool offset = (m_data[3] & 0xf8) == 0x80;
    return DescriptorLength() - ((offset) ? 2 : 1);
}

QString DishEventDescriptionDescriptor::Description(
    uint compression_type) const
{
    const unsigned char *raw = DescriptionRaw();
    uint len = DescriptionRawLength();

    if (raw && len)
        return atsc_huffman2_to_string(raw, len, compression_type);

    return QString();
}

bool DishEventPropertiesDescriptor::s_decompressed = false;
uint DishEventPropertiesDescriptor::s_subtitleProps = SUB_UNKNOWN;
uint DishEventPropertiesDescriptor::s_audioProps = AUD_UNKNOWN;

uint DishEventPropertiesDescriptor::SubtitleProperties(uint compression_type) const
{
    decompress_properties(compression_type);

    return s_subtitleProps;
}

uint DishEventPropertiesDescriptor::AudioProperties(uint compression_type) const
{
    decompress_properties(compression_type);

    return s_audioProps;
}

void DishEventPropertiesDescriptor::decompress_properties(uint compression_type) const
{
    s_subtitleProps = SUB_UNKNOWN;
    s_audioProps = AUD_UNKNOWN;

    if (HasProperties())
    {
        QString properties_raw = atsc_huffman2_to_string(
            m_data + 4, DescriptorLength() - 2, compression_type);

        if (properties_raw.contains("6|CC"))
            s_subtitleProps |= SUB_HARDHEAR;

        if (properties_raw.contains("7|Stereo"))
            s_audioProps    |= AUD_STEREO;
    }
}

QString DishEventTagsDescriptor::programid(void) const
{
    QString prefix = QString("");

    if (DescriptorLength() != 8)
        return QString();

    QString series = seriesid();
    series.remove(0, 2);

    uint episode = ((m_data[6] & 0x3f) << 0x08) | m_data[7];

    if (m_data[2] == 0x7c)
        prefix = "MV";
    else if (m_data[2] == 0x7d)
        prefix = "SP";
    else if (m_data[2] == 0x7e)
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
        return QString();

    if (m_data[2] == 0x7c)
        prefix = "MV";
    else if (m_data[2] == 0x7d)
        prefix = "SP";
    else if (m_data[2] == 0x7e)
        prefix = "EP";
    else
        return prefix;

    uint series  = (m_data[3] << 0x12) | (m_data[4] << 0x0a) | (m_data[5] << 0x02) |
                    ((m_data[6] & 0xc0) >> 0x06);

    QString id = QString("%1%2").arg(prefix).arg(series, 8, 0);

    return id;
}

QDate DishEventTagsDescriptor::originalairdate(void) const
{
    unsigned char mjd[5];

    if (DescriptorLength() != 8)
        return {};

    mjd[0] = m_data[8];
    mjd[1] = m_data[9];
    mjd[2] = 0;
    mjd[3] = 0;
    mjd[4] = 0;

    QDateTime t = dvbdate2qt(mjd);

    if (!t.isValid())
        return {};

    QDate originalairdate = t.date();

    if (originalairdate.year() < 1895)
        return {};

    return originalairdate;
}

QMutex             DishEventMPAADescriptor::s_mpaaRatingsLock;
QMap<uint,QString> DishEventMPAADescriptor::s_mpaaRatingsDesc;
bool               DishEventMPAADescriptor::s_mpaaRatingsExists = false;

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
    if (!s_mpaaRatingsExists)
        Init();

    QMutexLocker locker(&s_mpaaRatingsLock);

    QMap<uint,QString>::const_iterator it = s_mpaaRatingsDesc.find(rating_raw());
    if (it != s_mpaaRatingsDesc.end())
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
    QMutexLocker locker(&s_mpaaRatingsLock);

    if (s_mpaaRatingsExists)
        return;

    s_mpaaRatingsDesc[0x01] = "G";
    s_mpaaRatingsDesc[0x02] = "PG";
    s_mpaaRatingsDesc[0x03] = "PG-13";
    s_mpaaRatingsDesc[0x04] = "R";
    s_mpaaRatingsDesc[0x05] = "NC-17";
    s_mpaaRatingsDesc[0x06] = "NR";

    s_mpaaRatingsExists = true;
}

QMutex             DishEventVCHIPDescriptor::s_vchipRatingsLock;
QMap<uint,QString> DishEventVCHIPDescriptor::s_vchipRatingsDesc;
bool               DishEventVCHIPDescriptor::s_vchipRatingsExists = false;

QString DishEventVCHIPDescriptor::rating(void) const
{
    if (!s_vchipRatingsExists)
        Init();

    QMutexLocker locker(&s_vchipRatingsLock);

    QMap<uint,QString>::const_iterator it = s_vchipRatingsDesc.find(rating_raw());
    if (it != s_vchipRatingsDesc.end())
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
    QMutexLocker locker(&s_vchipRatingsLock);

    if (s_vchipRatingsExists)
        return;

    s_vchipRatingsDesc[0x01] = "TV-Y";
    s_vchipRatingsDesc[0x02] = "TV-Y7";
    s_vchipRatingsDesc[0x03] = "TV-G";
    s_vchipRatingsDesc[0x04] = "TV-PG";
    s_vchipRatingsDesc[0x05] = "TV-14";
    s_vchipRatingsDesc[0x06] = "TV-MA";

    s_vchipRatingsExists = true;
}

QMap<uint,QString> DishContentDescriptor::s_themeDesc;
QMap<uint,QString> DishContentDescriptor::s_dishCategoryDesc;
volatile bool      DishContentDescriptor::s_dishCategoryDescExists = false;

QString dish_theme_type_to_string(uint theme_type)
{
    // cppcheck-suppress variableScope
    static const char *s_themes[kThemeLast] =
    {
        "", "Movie", "Sports", "News/Business", "Family/Children", "Education",
        "Series/Special", "Music/Art", "Religious", "Off-Air"
    };

    if ((theme_type > kThemeNone) && (theme_type < kThemeLast))
        return QString(s_themes[theme_type]);

    return "";
}

DishThemeType string_to_dish_theme_type(const QString &theme_type)
{
    static const char *s_themes[kThemeLast] =
    {
        "", "Movie", "Sports", "News/Business", "Family/Children", "Education",
        "Series/Special", "Music/Art", "Religious", "Off-Air"
    };

    for (uint i = 1; i < 10; i++)
        if (theme_type == s_themes[i])
            return (DishThemeType) i;

    return kThemeNone;
}

DishThemeType DishContentDescriptor::GetTheme(void) const
{
    if (!s_dishCategoryDescExists)
        Init();

    if (Nibble1(0) == 0x00)
        return kThemeOffAir;

    QMap<uint,QString>::const_iterator it = s_themeDesc.find(Nibble2(0));
    if (it != s_themeDesc.end())
        return string_to_dish_theme_type(*it);

    // Found nothing? Just return empty string.
    return kThemeNone;
}

QString DishContentDescriptor::GetCategory(void) const
{
    if (!s_dishCategoryDescExists)
        Init();

    QMutexLocker locker(&s_categoryLock);

    // Try to get detailed description
    QMap<uint,QString>::const_iterator it =
        s_dishCategoryDesc.find(UserNibble(0));
    if (it != s_dishCategoryDesc.end())
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

    QMutexLocker locker(&s_categoryLock);

    if (s_dishCategoryDescExists)
        return;

    // Dish/BEV "Themes"
    s_themeDesc[kThemeMovie]     = QCoreApplication::translate("(Categories)",
                                                               "Movie");
    s_themeDesc[kThemeSports]    = QCoreApplication::translate("(Categories)",
                                                               "Sports");
    s_themeDesc[kThemeNews]      = QCoreApplication::translate("(Categories)",
                                                               "News/Business");
    s_themeDesc[kThemeFamily]    = QCoreApplication::translate("(Categories)",
                                                               "Family/Children");
    s_themeDesc[kThemeEducation] = QCoreApplication::translate("(Categories)",
                                                               "Education");
    s_themeDesc[kThemeSeries]    = QCoreApplication::translate("(Categories)",
                                                               "Series/Special");
    s_themeDesc[kThemeMusic]     = QCoreApplication::translate("(Categories)",
                                                               "Music/Art");
    s_themeDesc[kThemeReligious] = QCoreApplication::translate("(Categories)",
                                                               "Religious");

    // Dish/BEV categories
    s_dishCategoryDesc[0x01] = QCoreApplication::translate("(Categories)",
                                                           "Action");
    s_dishCategoryDesc[0x02] = QCoreApplication::translate("(Categories)",
                                                           "Adults only");
    s_dishCategoryDesc[0x03] = QCoreApplication::translate("(Categories)",
                                                           "Adventure");
    s_dishCategoryDesc[0x04] = QCoreApplication::translate("(Categories)",
                                                           "Animals");
    s_dishCategoryDesc[0x05] = QCoreApplication::translate("(Categories)",
                                                           "Animated");
    s_dishCategoryDesc[0x07] = QCoreApplication::translate("(Categories)",
                                                           "Anthology");
    s_dishCategoryDesc[0x08] = QCoreApplication::translate("(Categories)",
                                                           "Art");
    s_dishCategoryDesc[0x09] = QCoreApplication::translate("(Categories)",
                                                           "Auto");
    s_dishCategoryDesc[0x0a] = QCoreApplication::translate("(Categories)",
                                                           "Awards");
    s_dishCategoryDesc[0x0b] = QCoreApplication::translate("(Categories)",
                                                           "Ballet");
    s_dishCategoryDesc[0x0c] = QCoreApplication::translate("(Categories)",
                                                           "Baseball");
    s_dishCategoryDesc[0x0d] = QCoreApplication::translate("(Categories)",
                                                           "Basketball");
    s_dishCategoryDesc[0x11] = QCoreApplication::translate("(Categories)",
                                                           "Biography");
    s_dishCategoryDesc[0x12] = QCoreApplication::translate("(Categories)",
                                                           "Boat");
    s_dishCategoryDesc[0x14] = QCoreApplication::translate("(Categories)",
                                                           "Bowling");
    s_dishCategoryDesc[0x15] = QCoreApplication::translate("(Categories)",
                                                           "Boxing");
    s_dishCategoryDesc[0x16] = QCoreApplication::translate("(Categories)",
                                                           "Bus./financial");
    s_dishCategoryDesc[0x1a] = QCoreApplication::translate("(Categories)",
                                                           "Children");
    s_dishCategoryDesc[0x1b] = QCoreApplication::translate("(Categories)",
                                                           "Children-special");
    s_dishCategoryDesc[0x1c] = QCoreApplication::translate("(Categories)",
                                                           "Children-news");
    s_dishCategoryDesc[0x1d] = QCoreApplication::translate("(Categories)",
                                                           "Children-music");
    s_dishCategoryDesc[0x1f] = QCoreApplication::translate("(Categories)",
                                                           "Collectibles");
    s_dishCategoryDesc[0x20] = QCoreApplication::translate("(Categories)",
                                                           "Comedy");
    s_dishCategoryDesc[0x21] = QCoreApplication::translate("(Categories)",
                                                           "Comedy-drama");
    s_dishCategoryDesc[0x22] = QCoreApplication::translate("(Categories)",
                                                           "Computers");
    s_dishCategoryDesc[0x23] = QCoreApplication::translate("(Categories)",
                                                           "Cooking");
    s_dishCategoryDesc[0x24] = QCoreApplication::translate("(Categories)",
                                                           "Crime");
    s_dishCategoryDesc[0x25] = QCoreApplication::translate("(Categories)",
                                                           "Crime drama");
    s_dishCategoryDesc[0x27] = QCoreApplication::translate("(Categories)",
                                                           "Dance");
    s_dishCategoryDesc[0x29] = QCoreApplication::translate("(Categories)",
                                                           "Docudrama");
    s_dishCategoryDesc[0x2a] = QCoreApplication::translate("(Categories)",
                                                           "Documentary");
    s_dishCategoryDesc[0x2b] = QCoreApplication::translate("(Categories)",
                                                           "Drama");
    s_dishCategoryDesc[0x2c] = QCoreApplication::translate("(Categories)",
                                                           "Educational");
    s_dishCategoryDesc[0x2f] = QCoreApplication::translate("(Categories)",
                                                           "Exercise");
    s_dishCategoryDesc[0x31] = QCoreApplication::translate("(Categories)",
                                                           "Fantasy");
    s_dishCategoryDesc[0x32] = QCoreApplication::translate("(Categories)",
                                                           "Fashion");
    s_dishCategoryDesc[0x34] = QCoreApplication::translate("(Categories)",
                                                           "Fishing");
    s_dishCategoryDesc[0x35] = QCoreApplication::translate("(Categories)",
                                                           "Football");
    s_dishCategoryDesc[0x36] = QCoreApplication::translate("(Categories)",
                                                           "French");
    s_dishCategoryDesc[0x37] = QCoreApplication::translate("(Categories)",
                                                           "Fundraiser");
    s_dishCategoryDesc[0x38] = QCoreApplication::translate("(Categories)",
                                                           "Game show");
    s_dishCategoryDesc[0x39] = QCoreApplication::translate("(Categories)",
                                                           "Golf");
    s_dishCategoryDesc[0x3a] = QCoreApplication::translate("(Categories)",
                                                           "Gymnastics");
    s_dishCategoryDesc[0x3b] = QCoreApplication::translate("(Categories)",
                                                           "Health");
    s_dishCategoryDesc[0x3c] = QCoreApplication::translate("(Categories)",
                                                           "History");
    s_dishCategoryDesc[0x3d] = QCoreApplication::translate("(Categories)",
                                                           "Historical drama");
    s_dishCategoryDesc[0x3e] = QCoreApplication::translate("(Categories)",
                                                           "Hockey");
    s_dishCategoryDesc[0x3f] = QCoreApplication::translate("(Categories)",
                                                           "Holiday");
    s_dishCategoryDesc[0x40] = QCoreApplication::translate("(Categories)",
                                                           "Holiday-children");
    s_dishCategoryDesc[0x41] = QCoreApplication::translate("(Categories)",
                                                           "Holiday-children special");
    s_dishCategoryDesc[0x44] = QCoreApplication::translate("(Categories)",
                                                           "Holiday special");
    s_dishCategoryDesc[0x45] = QCoreApplication::translate("(Categories)",
                                                           "Horror");
    s_dishCategoryDesc[0x46] = QCoreApplication::translate("(Categories)",
                                                           "Horse racing");
    s_dishCategoryDesc[0x47] = QCoreApplication::translate("(Categories)",
                                                           "House/garden");
    s_dishCategoryDesc[0x49] = QCoreApplication::translate("(Categories)",
                                                           "How-to");
    s_dishCategoryDesc[0x4b] = QCoreApplication::translate("(Categories)",
                                                           "Interview");
    s_dishCategoryDesc[0x4d] = QCoreApplication::translate("(Categories)",
                                                           "Lacrosse");
    s_dishCategoryDesc[0x4f] = QCoreApplication::translate("(Categories)",
                                                           "Martial Arts");
    s_dishCategoryDesc[0x50] = QCoreApplication::translate("(Categories)",
                                                           "Medical");
    s_dishCategoryDesc[0x51] = QCoreApplication::translate("(Categories)",
                                                           "Miniseries");
    s_dishCategoryDesc[0x52] = QCoreApplication::translate("(Categories)",
                                                           "Motorsports");
    s_dishCategoryDesc[0x53] = QCoreApplication::translate("(Categories)",
                                                           "Motorcycle");
    s_dishCategoryDesc[0x54] = QCoreApplication::translate("(Categories)",
                                                           "Music");
    s_dishCategoryDesc[0x55] = QCoreApplication::translate("(Categories)",
                                                           "Music special");
    s_dishCategoryDesc[0x56] = QCoreApplication::translate("(Categories)",
                                                           "Music talk");
    s_dishCategoryDesc[0x57] = QCoreApplication::translate("(Categories)",
                                                           "Musical");
    s_dishCategoryDesc[0x58] = QCoreApplication::translate("(Categories)",
                                                           "Musical comedy");
    s_dishCategoryDesc[0x5a] = QCoreApplication::translate("(Categories)",
                                                           "Mystery");
    s_dishCategoryDesc[0x5b] = QCoreApplication::translate("(Categories)",
                                                           "Nature");
    s_dishCategoryDesc[0x5c] = QCoreApplication::translate("(Categories)",
                                                           "News");
    s_dishCategoryDesc[0x5f] = QCoreApplication::translate("(Categories)",
                                                           "Opera");
    s_dishCategoryDesc[0x60] = QCoreApplication::translate("(Categories)",
                                                           "Outdoors");
    s_dishCategoryDesc[0x63] = QCoreApplication::translate("(Categories)",
                                                           "Public affairs");
    s_dishCategoryDesc[0x66] = QCoreApplication::translate("(Categories)",
                                                           "Reality");
    s_dishCategoryDesc[0x67] = QCoreApplication::translate("(Categories)",
                                                           "Religious");
    s_dishCategoryDesc[0x68] = QCoreApplication::translate("(Categories)",
                                                           "Rodeo");
    s_dishCategoryDesc[0x69] = QCoreApplication::translate("(Categories)",
                                                           "Romance");
    s_dishCategoryDesc[0x6a] = QCoreApplication::translate("(Categories)",
                                                           "Romance-comedy");
    s_dishCategoryDesc[0x6b] = QCoreApplication::translate("(Categories)",
                                                           "Rugby");
    s_dishCategoryDesc[0x6c] = QCoreApplication::translate("(Categories)",
                                                           "Running");
    s_dishCategoryDesc[0x6e] = QCoreApplication::translate("(Categories)",
                                                           "Science");
    s_dishCategoryDesc[0x6f] = QCoreApplication::translate("(Categories)",
                                                           "Science fiction");
    s_dishCategoryDesc[0x70] = QCoreApplication::translate("(Categories)",
                                                           "Self improvement");
    s_dishCategoryDesc[0x71] = QCoreApplication::translate("(Categories)",
                                                           "Shopping");
    s_dishCategoryDesc[0x74] = QCoreApplication::translate("(Categories)",
                                                           "Skiing");
    s_dishCategoryDesc[0x77] = QCoreApplication::translate("(Categories)",
                                                           "Soap");
    s_dishCategoryDesc[0x7b] = QCoreApplication::translate("(Categories)",
                                                           "Soccer");
    s_dishCategoryDesc[0x7c] = QCoreApplication::translate("(Categories)",
                                                           "Softball");
    s_dishCategoryDesc[0x7d] = QCoreApplication::translate("(Categories)",
                                                           "Spanish");
    s_dishCategoryDesc[0x7e] = QCoreApplication::translate("(Categories)",
                                                           "Special");
    s_dishCategoryDesc[0x81] = QCoreApplication::translate("(Categories)",
                                                           "Sports non-event");
    s_dishCategoryDesc[0x82] = QCoreApplication::translate("(Categories)",
                                                           "Sports talk");
    s_dishCategoryDesc[0x83] = QCoreApplication::translate("(Categories)",
                                                           "Suspense");
    s_dishCategoryDesc[0x85] = QCoreApplication::translate("(Categories)",
                                                           "Swimming");
    s_dishCategoryDesc[0x86] = QCoreApplication::translate("(Categories)",
                                                           "Talk");
    s_dishCategoryDesc[0x87] = QCoreApplication::translate("(Categories)",
                                                           "Tennis");
    s_dishCategoryDesc[0x89] = QCoreApplication::translate("(Categories)",
                                                           "Track/field");
    s_dishCategoryDesc[0x8a] = QCoreApplication::translate("(Categories)",
                                                           "Travel");
    s_dishCategoryDesc[0x8b] = QCoreApplication::translate("(Categories)",
                                                           "Variety");
    s_dishCategoryDesc[0x8c] = QCoreApplication::translate("(Categories)",
                                                           "Volleyball");
    s_dishCategoryDesc[0x8d] = QCoreApplication::translate("(Categories)",
                                                           "War");
    s_dishCategoryDesc[0x8e] = QCoreApplication::translate("(Categories)",
                                                           "Watersports");
    s_dishCategoryDesc[0x8f] = QCoreApplication::translate("(Categories)",
                                                           "Weather");
    s_dishCategoryDesc[0x90] = QCoreApplication::translate("(Categories)",
                                                           "Western");
    s_dishCategoryDesc[0x92] = QCoreApplication::translate("(Categories)",
                                                           "Wrestling");
    s_dishCategoryDesc[0x93] = QCoreApplication::translate("(Categories)",
                                                           "Yoga");
    s_dishCategoryDesc[0x94] = QCoreApplication::translate("(Categories)",
                                                           "Agriculture");
    s_dishCategoryDesc[0x95] = QCoreApplication::translate("(Categories)",
                                                           "Anime");
    s_dishCategoryDesc[0x97] = QCoreApplication::translate("(Categories)",
                                                           "Arm Wrestling");
    s_dishCategoryDesc[0x98] = QCoreApplication::translate("(Categories)",
                                                           "Arts/crafts");
    s_dishCategoryDesc[0x99] = QCoreApplication::translate("(Categories)",
                                                           "Auction");
    s_dishCategoryDesc[0x9a] = QCoreApplication::translate("(Categories)",
                                                           "Auto racing");
    s_dishCategoryDesc[0x9b] = QCoreApplication::translate("(Categories)",
                                                           "Air racing");
    s_dishCategoryDesc[0x9c] = QCoreApplication::translate("(Categories)",
                                                           "Badminton");
    s_dishCategoryDesc[0xa0] = QCoreApplication::translate("(Categories)",
                                                           "Bicycle racing");
    s_dishCategoryDesc[0xa1] = QCoreApplication::translate("(Categories)",
                                                           "Boat Racing");
    s_dishCategoryDesc[0xa6] = QCoreApplication::translate("(Categories)",
                                                           "Community");
    s_dishCategoryDesc[0xa7] = QCoreApplication::translate("(Categories)",
                                                           "Consumer");
    s_dishCategoryDesc[0xaa] = QCoreApplication::translate("(Categories)",
                                                           "Debate");
    s_dishCategoryDesc[0xac] = QCoreApplication::translate("(Categories)",
                                                           "Dog show");
    s_dishCategoryDesc[0xad] = QCoreApplication::translate("(Categories)",
                                                           "Drag racing");
    s_dishCategoryDesc[0xae] = QCoreApplication::translate("(Categories)",
                                                           "Entertainment");
    s_dishCategoryDesc[0xaf] = QCoreApplication::translate("(Categories)",
                                                           "Environment");
    s_dishCategoryDesc[0xb0] = QCoreApplication::translate("(Categories)",
                                                           "Equestrian");
    s_dishCategoryDesc[0xb3] = QCoreApplication::translate("(Categories)",
                                                           "Field hockey");
    s_dishCategoryDesc[0xb5] = QCoreApplication::translate("(Categories)",
                                                           "Football");
    s_dishCategoryDesc[0xb6] = QCoreApplication::translate("(Categories)",
                                                           "Gay/lesbian");
    s_dishCategoryDesc[0xb7] = QCoreApplication::translate("(Categories)",
                                                           "Handball");
    s_dishCategoryDesc[0xb8] = QCoreApplication::translate("(Categories)",
                                                           "Home improvement");
    s_dishCategoryDesc[0xb9] = QCoreApplication::translate("(Categories)",
                                                           "Hunting");
    s_dishCategoryDesc[0xbb] = QCoreApplication::translate("(Categories)",
                                                           "Hydroplane racing");
    s_dishCategoryDesc[0xc1] = QCoreApplication::translate("(Categories)",
                                                           "Law");
    s_dishCategoryDesc[0xc3] = QCoreApplication::translate("(Categories)",
                                                           "Motorcycle racing");
    s_dishCategoryDesc[0xc5] = QCoreApplication::translate("(Categories)",
                                                           "Newsmagazine");
    s_dishCategoryDesc[0xc7] = QCoreApplication::translate("(Categories)",
                                                           "Paranormal");
    s_dishCategoryDesc[0xc8] = QCoreApplication::translate("(Categories)",
                                                           "Parenting");
    s_dishCategoryDesc[0xca] = QCoreApplication::translate("(Categories)",
                                                           "Performing arts");
    s_dishCategoryDesc[0xcc] = QCoreApplication::translate("(Categories)",
                                                           "Politics");
    s_dishCategoryDesc[0xcf] = QCoreApplication::translate("(Categories)",
                                                           "Pro wrestling");
    s_dishCategoryDesc[0xd3] = QCoreApplication::translate("(Categories)",
                                                           "Sailing");
    s_dishCategoryDesc[0xd4] = QCoreApplication::translate("(Categories)",
                                                           "Shooting");
    s_dishCategoryDesc[0xd5] = QCoreApplication::translate("(Categories)",
                                                           "Sitcom");
    s_dishCategoryDesc[0xd6] = QCoreApplication::translate("(Categories)",
                                                           "Skateboarding");
    s_dishCategoryDesc[0xd9] = QCoreApplication::translate("(Categories)",
                                                           "Snowboarding");
    s_dishCategoryDesc[0xdd] = QCoreApplication::translate("(Categories)",
                                                           "Standup");
    s_dishCategoryDesc[0xdf] = QCoreApplication::translate("(Categories)",
                                                           "Surfing");
    s_dishCategoryDesc[0xe0] = QCoreApplication::translate("(Categories)",
                                                           "Tennis");
    s_dishCategoryDesc[0xe1] = QCoreApplication::translate("(Categories)",
                                                           "Triathlon");
    s_dishCategoryDesc[0xe6] = QCoreApplication::translate("(Categories)",
                                                           "Card games");
    s_dishCategoryDesc[0xe7] = QCoreApplication::translate("(Categories)",
                                                           "Poker");
    s_dishCategoryDesc[0xea] = QCoreApplication::translate("(Categories)",
                                                           "Military");
    s_dishCategoryDesc[0xeb] = QCoreApplication::translate("(Categories)",
                                                           "Technology");
    s_dishCategoryDesc[0xec] = QCoreApplication::translate("(Categories)",
                                                           "Mixed martial arts");
    s_dishCategoryDesc[0xed] = QCoreApplication::translate("(Categories)",
                                                           "Action sports");
    s_dishCategoryDesc[0xff] = QCoreApplication::translate("(Categories)",
                                                           "Dish Network");

    s_dishCategoryDescExists = true;
}
