#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
using namespace std;

#include <QObject>
#include <QIODevice>
#include <QDir>
#include <QFile>

#include <mythconfig.h>   // For CONFIG_DARWIN
#include "cddecoder.h"
#include "constants.h"
#include <audiooutput.h>
#include "metadata.h"

#include <mythcontext.h>
#include <mythmediamonitor.h>
#include <mythversion.h>
#include <httpcomms.h>

CdDecoder::CdDecoder(const QString &file, DecoderFactory *d, QIODevice *i,
                     AudioOutput *o)
         : Decoder(d, i, o)
{
    filename = file;
    inited = FALSE;
}

CdDecoder::~CdDecoder(void)
{
    if (inited)
        deinit();
}

/*
 * Helper function for CD checksum generation
 */
static inline int addDecimalDigits(int i)
{
    int total = 0;
    while (i > 0)
        total += i % 10, i /= 10;
    return total;
}

/*
 * Work out file containing a given track
 */
QString fileForTrack(QString path, uint track)
{
    QDir    disc(path);
    QString filename;

    disc.setNameFilter(QString("%1*.aiff").arg(track));
    filename = disc.entryList()[0];  // Fortunately, this seems to sort nicely

    if (filename.isEmpty())
        filename = QString("%1.aiff").arg(track);

    return filename;
}

/**
 * Load XML file that OS X generates for us for Audio CDs, calculate checksum
 */
bool CdDecoder::initialize()
{
    QFile        TOCfile(devicename + "/.TOC.plist");
    QDomDocument TOC;
    uint         trk;

    m_tracks.clear();
    m_firstTrack = m_lastTrack = m_leadout = 0;

    if (!TOCfile.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_GENERAL,
                "Unable to open Audio CD TOC file: " + TOCfile.fileName());
        return false;
    }

    if (!TOC.setContent(&TOCfile))
    {
        VERBOSE(VB_GENERAL,
                "Unable to parse Audio CD TOC file: " + TOCfile.fileName());
        TOCfile.close();
        return false;
    }


    // HACK. This is a really bad example of XML parsing. No type checking,
    // it doesn't deal with comments. It only works because the TOC.plist
    // file is generated (i.e. a fixed format)

    QDomElement root = TOC.documentElement();
    QDomNode    node = root.firstChild()        // <dict>
                           .namedItem("array")  //   <key>Sessions</key><array>
                           .firstChild()        //     <dict>
                           .firstChild();
    while (!node.isNull())
    {
        if (node.nodeName() == "key")
        {
            QDomText t = node.firstChild().toText();  // <key>  t  </key>
            node       = node.nextSibling();          // <integer>i</integer>
            int      i = node.firstChild().toText()
                             .data().toInt();

            if (t.data() == "First Track")
                m_firstTrack = i;
            if (t.data() == "Last Track")
                m_lastTrack = i;
            if (t.data() == "Leadout Block")
                m_leadout = i;
        }
                                         // <key>Track Array</key>
        if (node.nodeName() == "array")  // <array>
        {
            node = node.firstChild();    // First track's <dict>

            for (trk = m_firstTrack; trk <= m_lastTrack; ++trk)
            {
                m_tracks.push_back(node.lastChild().firstChild()
                                       .toText().data().toInt());

                node = node.nextSibling();  // Look at next <dict> in <array>
            }
        }

        node = node.nextSibling();
    }
    TOCfile.close();


    // Calculate some stuff for later CDDB/FreeDB lookup

    m_lengthInSecs = (m_leadout - m_tracks[0]) / 75.0;

    int checkSum = 0;
    for (trk = 0; trk <= m_lastTrack - m_firstTrack; ++trk)
        checkSum += addDecimalDigits(m_tracks[trk] / 75);

    uint totalTracks = 1 + m_lastTrack - m_firstTrack;
    m_diskID = ((checkSum % 255) << 24) | (int)m_lengthInSecs << 8
                                        | totalTracks;

    QString hexID;
    hexID.setNum(m_diskID, 16);
    VERBOSE(VB_MEDIA, QString("CD %1, ID=%2").arg(devicename).arg(hexID));


    // First erase any existing metadata:
    for (trk = 0; trk < m_mData.size(); ++trk)
        delete m_mData[trk];
    m_mData.clear();


    // Generate empty MetaData records.
    // We fill in the other details later (from CDDB if possible)

    m_tracks.push_back(m_leadout);  // This simplifies the loop

    for (trk = 1; trk <= totalTracks; ++trk)
    {
        QString file = fileForTrack(devicename, trk);
        uint    len  = 1000 * (m_tracks[trk] - m_tracks[trk-1]) / 75;

        m_mData.push_back(new Metadata(file, NULL, NULL, NULL,
                                       NULL, NULL, 0, trk, len));
    }


    // Try to fill in this MetaData from CDDB lookup:
    lookupCDDB(hexID, totalTracks);


    inited = true;
    return true;
}

/**
 * Lookup FreeDB/CDDB, populate m_mData with results
 */
void CdDecoder::lookupCDDB(const QString &hexID, uint totalTracks)
{
    QString helloID = getenv("USER");
    QString queryID = "cddb+query+";
    uint    trk;

    if (helloID.isEmpty())
        helloID = "anon";
    helloID += QString("+%1+MythTV+%2+")
               .arg(gCoreContext->GetHostName()).arg(MYTH_BINARY_VERSION);

    queryID += QString("%1+%2+").arg(hexID).arg(totalTracks);
    for (trk = 0; trk < totalTracks; ++trk)
        queryID += QString::number(m_tracks[trk]) + '+';
    queryID += QString::number(m_lengthInSecs);


    // First, try HTTP:
    QString URL  = "http://freedb.freedb.org/~cddb/cddb.cgi?cmd=";
    QString URL2 = URL + queryID + "&hello=" + helloID + "&proto=5";
    QString cddb = HttpComms::getHttp(URL2);

    //VERBOSE(VB_MEDIA, "CDDB lookup: " + URL);
    //VERBOSE(VB_MEDIA, "...returned: " + cddb);
    //
    // e.g. "200 rock 960b5e0c Nichole Nordeman / Woven & Spun"

    // Extract/remove 3 digit status:
    uint stat = cddb.left(3).toUInt();
    cddb = cddb.mid(4);

    // We should check for errors here, and possibly do a CDDB lookup
    // (telnet 8880) if it failed, but Nigel is feeling lazy.

    if (stat == 211)  // Multiple matches
    {
        // Parse disks, put up dialog box, select disk, prune cddb to selected
        VERBOSE(VB_MEDIA, "Multiple CDDB matches. Please implement this code");
    }

    if (stat == 200)  // One unique match
    {
        QString album;
        QString artist;
        bool    compn = false;
        QString genre = cddb.section(' ', 0, 0);
        int     year  = 0;

        // Now we can lookup all its details:
        URL2 = URL + "cddb+read+" + genre + "+"
               + hexID + "&hello=" + helloID + "&proto=5";
        cddb = HttpComms::getHttp(URL2);
        //VERBOSE(VB_MEDIA, "CDDB detail: " + URL2);
        //VERBOSE(VB_MEDIA, "...returned: " + cddb);

        // Successful lookup.
        // Clear current titles (filenames), because we append to them
        for (trk = 0; trk < totalTracks; ++trk)
            m_mData[trk]->setTitle("");

        // Parse returned data:

        cddb.replace(QRegExp(".*#"), "");  // Remove comment block
        while (cddb.length())
        {
            // Lines should be of the form "FIELD=value\r\n"

            QString art;
            QString line  = cddb.section(QRegExp("(\r|\n)+"), 0, 0);
            QString value = line.section('=', 1, 1);

            if (value.contains(" / "))
            {
                art   = value.section(" / ", 0, 0);  // Artist in *TITLE
                value = value.section(" / ", 1, 1);
            }

            if (line.startsWith("DGENRE="))
                genre = value;
            else if (line.startsWith("DYEAR="))
                year = value.toInt();
            else if (line.startsWith("DTITLE="))
            {
                // Albums (and maybe artists?) can wrap over multiple lines:
                artist += art;
                album  += value;
            }
            else if (line.startsWith("TTITLE"))
            {
                trk = line.remove("TTITLE").remove(QRegExp("=.*")).toUInt();

                if (trk < totalTracks)
                {
                    Metadata *m = m_mData[trk];

                    // Titles can wrap over multiple lines, so we load+store:
                    m->setTitle(m->Title() + value);

                    if (art.length())
                    {
                        compn = true;  // Probably a compilation

                        m->setArtist(M_QSTRING_UNICODE(art));
                    }
                }
                else
                    VERBOSE(VB_GENERAL,
                            QString("CDDB returned %1 on a %2 track disk!")
                            .arg(trk+1).arg(totalTracks));
            }

            // Get next THINGY=value line:
            cddb = cddb.section('\n', 1, 0xffffffff);
        }

        for (trk = 0; trk < totalTracks; ++trk)
        {
            Metadata *m = m_mData[trk];

            if (compn)
                m->setCompilation(true);

            m->setGenre(M_QSTRING_UNICODE(genre));

            if (year)
                m->setYear(year);

            if (album.length())
                m->setAlbum(M_QSTRING_UNICODE(album));

            if (artist.length())
                if (compn)
                    m->setCompilationArtist(M_QSTRING_UNICODE(artist));
                else
                    m->setArtist(M_QSTRING_UNICODE(artist));
        }
    }
}

double CdDecoder::lengthInSeconds()
{
    return m_lengthInSecs;
}

void CdDecoder::seek(double pos)
{
    (void)pos;
}

void CdDecoder::stop()
{
}

void CdDecoder::run()
{
}

void CdDecoder::flush(bool final)
{
    (void)final;
}

void CdDecoder::deinit()
{
    // Free any stored Metadata
    for (unsigned int i = 0; i < m_mData.size(); ++i)
        delete m_mData[i];
    m_mData.clear();

    m_tracks.clear();

    inited = false;
}

int CdDecoder::getNumTracks(void)
{
    if (!inited && !initialize())
        return 0;

    return m_lastTrack;
}

int CdDecoder::getNumCDAudioTracks(void)
{
    if (!inited && !initialize())
        return 0;

    return m_lastTrack - m_firstTrack + 1;
}

Metadata* CdDecoder::getMetadata(int track)
{
    if (!inited && !initialize())
        return NULL;

    if (track < 1 || (uint)track > m_mData.size())
    {
        VERBOSE(VB_GENERAL,
                QString("CdDecoder::getMetadata(%1) - track out of range")
                .arg(track));
        return NULL;
    }

    return new Metadata(*(m_mData[track - 1]));
}

Metadata *CdDecoder::getLastMetadata()
{
    if (!inited && !initialize())
        return NULL;

    return new Metadata(*(m_mData[m_mData.size() - 1]));
}

Metadata *CdDecoder::getMetadata()
{
    return NULL;
}

void CdDecoder::commitMetadata(Metadata *mdata)
{
    (void)mdata;
}

bool CdDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}

const QString &CdDecoderFactory::extension() const
{
    static QString ext(".aiff");
    return ext;
}


const QString &CdDecoderFactory::description() const
{
    static QString desc(QObject::tr("OSX Audio CD mount parser"));
    return desc;
}

Decoder *CdDecoderFactory::create(const QString &file, QIODevice *input,
                                  AudioOutput *output, bool deletable)
{
    if (deletable)
        return new CdDecoder(file, this, input, output);

    static CdDecoder *decoder = 0;
    if (! decoder) {
        decoder = new CdDecoder(file, this, input, output);
    } else {
        decoder->setInput(input);
        decoder->setFilename(file);
        decoder->setOutput(output);
    }

    return decoder;
}
