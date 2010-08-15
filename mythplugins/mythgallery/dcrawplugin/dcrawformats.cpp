#include "dcrawformats.h"

#include <QSet>
#include <QString>

namespace {

QSet<QString> composeFormats()
{
    QSet<QString> formats;
    formats << "arw" << "ARW";
    formats << "bay" << "BAY";
    formats << "bmq" << "BMQ";
    formats << "cr2" << "CR2";
    formats << "crw" << "CRW";
    formats << "cs1" << "CS1";
    formats << "dc2" << "DC2";
    formats << "dcr" << "DCR";
    formats << "dng" << "DNG";
    formats << "fff" << "FFF";
    formats << "k25" << "K25";
    formats << "kdc" << "KDC";
    formats << "mos" << "MOS";
    formats << "mrw" << "MRW";
    formats << "nef" << "NEF";
    formats << "orf" << "ORF";
    formats << "pef" << "PEF";
    formats << "raf" << "RAF";
    formats << "raw" << "RAW";
    formats << "rdc" << "RDC";
    formats << "srf" << "SRF";
    formats << "x3f" << "X3F";
    return formats;
}

}    // anonymous namespace

QSet<QString> DcrawFormats::getFormats()
{
    static QSet<QString> formats(composeFormats());
    return formats;
}

QStringList DcrawFormats::getFilters()
{
    QSet<QString> formats(getFormats());
    QStringList filters;
    for (QSet<QString>::const_iterator i(formats.begin()); i != formats.end(); ++i)
        filters << ("*." + *i);
    return filters;
}

