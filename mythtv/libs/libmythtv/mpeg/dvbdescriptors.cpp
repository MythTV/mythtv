#include "dvbdescriptors.h"

#include <qtextcodec.h>
#include <qdeepcopy.h>

// Decode a text string according to ETSI EN 300 468 Annex A
QString dvb_decode_text(const unsigned char *src, uint length)
{
    QString result;

    if (!length)
        return QString("");

    unsigned char buf[length];
    memcpy(buf, src, length);

    if ((buf[0] <= 0x10) || (buf[0] >= 0x20))
    {
        // Strip formatting characters
        for (uint p = 0; p < length; p++)
        {
            if ((buf[p] >= 0x80) && (buf[p] <= 0x9F))
                memmove(buf + p, buf + p + 1, --length - p);
        }

        if (buf[0] >= 0x20)
        {
            result = QString::fromLatin1((const char*)buf, length);
        }
        else if ((buf[0] >= 0x01) && (buf[0] <= 0x0B))
        {
            QString coding = "ISO8859-" + QString::number(4 + buf[0]);
            QTextCodec *codec = QTextCodec::codecForName(coding);
            result = codec->toUnicode((const char*)buf + 1, length - 1);
        }
        else if (buf[0] == 0x10)
        {
            // If the first byte of the text field has a value "0x10"
            // then the following two bytes carry a 16-bit value (uimsbf) N
            // to indicate that the remaining data of the text field is
            // coded using the character code table specified by
            // ISO Standard 8859, parts 1 to 9

            uint code = 1;
            swab(buf + 1, &code, 2);
            QString coding = "ISO8859-" + QString::number(code);
            QTextCodec *codec = QTextCodec::codecForName(coding);
            result = codec->toUnicode((const char*)buf + 3, length - 3);
        }
        else
        {
            // Unknown/invalid encoding - assume local8Bit
            result = QString::fromLocal8Bit((const char*)buf + 1, length - 1);
        }
    }
    else
    {
        // TODO: Handle multi-byte encodings

        VERBOSE(VB_SIPARSER, "dvbdescriptors.cpp: "
                "Multi-byte coded text - not supported!");
        result = "N/A";
    }

    return result;
}

QMutex            ContentDescriptor::categoryLock;
map<uint,QString> ContentDescriptor::categoryDesc;
bool              ContentDescriptor::categoryDescExists = false;

QString ContentDescriptor::GetMythCategory(uint i) const
{
    if (0x1 == Nibble1(i))
        return "movie";
    if (0x4 == Nibble1(i))
        return "sports";
    return "tvshow";
}

QString ContentDescriptor::GetDescription(uint i) const
{
    if (!categoryDescExists)
        Init();

    QMutexLocker locker(&categoryLock);

    // Try to get detailed description
    map<uint,QString>::const_iterator it = categoryDesc.find(Nibble(i));
    if (it != categoryDesc.end())
        return QDeepCopy<QString>((*it).second);

    // Fall back to category description
    it = categoryDesc.find(Nibble1(i)<<4);
    if (it != categoryDesc.end())
        return QDeepCopy<QString>((*it).second);

    // Found nothing? Just return empty string.
    return "";
}

QString ContentDescriptor::toString() const
{
    QString tmp("");
    for (uint i = 0; i < Count(); i++)
        tmp += GetMythCategory(i) + " : " + GetDescription(i);
    return tmp;
}

void ContentDescriptor::Init(void)
{
    QMutexLocker locker(&categoryLock);

    if (categoryDescExists)
        return;

    categoryDesc[0x10] = QObject::tr("Movie");
    categoryDesc[0x11] = QObject::tr("Movie") + " - " +
        QObject::tr("Detective/Thriller");
    categoryDesc[0x12] = QObject::tr("Movie")+ " - " +
        QObject::tr("Adventure/Western/War");
    categoryDesc[0x13] = QObject::tr("Movie")+ " - " +
        QObject::tr("Science Fiction/Fantasy/Horror");
    categoryDesc[0x14] = QObject::tr("Movie")+ " - " +
        QObject::tr("Comedy");
    categoryDesc[0x15] = QObject::tr("Movie")+ " - " +
        QObject::tr("Soap/melodrama/folkloric");
    categoryDesc[0x16] = QObject::tr("Movie")+ " - " +
        QObject::tr("Romance");
    categoryDesc[0x17] = QObject::tr("Movie")+ " - " +
        QObject::tr("Serious/Classical/Religious/Historical Movie/Drama");
    categoryDesc[0x18] = QObject::tr("Movie")+ " - " +
        QObject::tr("Adult", "Adult Movie");
    
    categoryDesc[0x20] = QObject::tr("News");
    categoryDesc[0x21] = QObject::tr("News/weather report");
    categoryDesc[0x22] = QObject::tr("News magazine");
    categoryDesc[0x23] = QObject::tr("Documentary");
    categoryDesc[0x24] = QObject::tr("Intelligent Programmes");
    
    categoryDesc[0x30] = QObject::tr("Show/game Show");
    categoryDesc[0x31] = QObject::tr("Game Show");
    categoryDesc[0x32] = QObject::tr("Variety Show");
    categoryDesc[0x33] = QObject::tr("Talk Show");
    
    categoryDesc[0x40] = QObject::tr("Sports");
    categoryDesc[0x41] =
        QObject::tr("Special Events (World Cup, World Series..)");
    categoryDesc[0x42] = QObject::tr("Sports Magazines");
    categoryDesc[0x43] = QObject::tr("Football (Soccer)");
    categoryDesc[0x44] = QObject::tr("Tennis/Squash");
    categoryDesc[0x45] =
        QObject::tr("Misc. Team Sports"); // not football/soccer
    categoryDesc[0x46] = QObject::tr("Athletics");
    categoryDesc[0x47] = QObject::tr("Motor Sport");
    categoryDesc[0x48] = QObject::tr("Water Sport");
    categoryDesc[0x49] = QObject::tr("Winter Sports");
    categoryDesc[0x4A] = QObject::tr("Equestrian");
    categoryDesc[0x4B] = QObject::tr("Martial Sports");
    
    categoryDesc[0x50] = QObject::tr("Kids");
    categoryDesc[0x51] = QObject::tr("Pre-School Children's Programmes");
    categoryDesc[0x52] = QObject::tr("Entertainment Programmes for 6 to 14");
    categoryDesc[0x53] = QObject::tr("Entertainment Programmes for 10 to 16");
    categoryDesc[0x54] = QObject::tr("Informational/Educational");
    categoryDesc[0x55] = QObject::tr("Cartoons/Puppets");
    
    categoryDesc[0x60] = QObject::tr("Music/Ballet/Dance");
    categoryDesc[0x61] = QObject::tr("Rock/Pop");
    categoryDesc[0x62] = QObject::tr("Classical Music");
    categoryDesc[0x63] = QObject::tr("Folk Music");
    categoryDesc[0x64] = QObject::tr("Jazz");
    categoryDesc[0x65] = QObject::tr("Musical/Opera");
    categoryDesc[0x66] = QObject::tr("Ballet");

    categoryDesc[0x70] = QObject::tr("Arts/Culture");
    categoryDesc[0x71] = QObject::tr("Performing Arts");
    categoryDesc[0x72] = QObject::tr("Fine Arts");
    categoryDesc[0x73] = QObject::tr("Religion");
    categoryDesc[0x74] = QObject::tr("Popular Culture/Traditional Arts");
    categoryDesc[0x75] = QObject::tr("Literature");
    categoryDesc[0x76] = QObject::tr("Film/Cinema");
    categoryDesc[0x77] = QObject::tr("Experimental Film/Video");
    categoryDesc[0x78] = QObject::tr("Broadcasting/Press");
    categoryDesc[0x79] = QObject::tr("New Media");
    categoryDesc[0x7A] = QObject::tr("Arts/Culture Magazines");
    categoryDesc[0x7B] = QObject::tr("Fashion");
    
    categoryDesc[0x80] = QObject::tr("Social/Policical/Economics");
    categoryDesc[0x81] = QObject::tr("Magazines/Reports/Documentary");
    categoryDesc[0x82] = QObject::tr("Economics/Social Advisory");
    categoryDesc[0x83] = QObject::tr("Remarkable People");
    
    categoryDesc[0x90] = QObject::tr("Education/Science/Factual");
    categoryDesc[0x91] = QObject::tr("Nature/animals/Environment");
    categoryDesc[0x92] = QObject::tr("Technology/Natural Sciences");
    categoryDesc[0x93] = QObject::tr("Medicine/Physiology/Psychology");
    categoryDesc[0x94] = QObject::tr("Foreign Countries/Expeditions");
    categoryDesc[0x95] = QObject::tr("Social/Spiritual Sciences");
    categoryDesc[0x96] = QObject::tr("Further Education");
    categoryDesc[0x97] = QObject::tr("Languages");
    
    categoryDesc[0xA0] = QObject::tr("Leisure/Hobbies");
    categoryDesc[0xA1] = QObject::tr("Tourism/Travel");
    categoryDesc[0xA2] = QObject::tr("Handicraft");
    categoryDesc[0xA3] = QObject::tr("Motoring");
    categoryDesc[0xA4] = QObject::tr("Fitness & Health");
    categoryDesc[0xA5] = QObject::tr("Cooking");
    categoryDesc[0xA6] = QObject::tr("Advertizement/Shopping");
    categoryDesc[0xA7] = QObject::tr("Gardening");
    // Special
    categoryDesc[0xB0] = QObject::tr("Original Language");
    categoryDesc[0xB1] = QObject::tr("Black & White");
    categoryDesc[0xB2] = QObject::tr("\"Unpublished\" Programmes");
    categoryDesc[0xB3] = QObject::tr("Live Broadcast");
    // UK Freeview custom id
    categoryDesc[0xF0] = QObject::tr("Drama");

    categoryDescExists = true;
}
