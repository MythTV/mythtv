#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
#include <qfile.h>
using namespace std;

#include <mythtv/mythconfig.h>   // For CONFIG_DARWIN
#include "cddecoder.h"
#include "constants.h"
#include <mythtv/audiooutput.h>
#include "metadata.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythmediamonitor.h>
//#include <mythtv/xmlparse.h>

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

/**
 * Load XML file that OS X generates for us for Audio CDs, and calc. checksum
 */
bool CdDecoder::initialize()
{
    QFile        TOCfile(devicename + "/.TOC.plist");
    QDomDocument TOC;

    if (!TOCfile.open(IO_ReadOnly))
    {
        VERBOSE(VB_GENERAL, "Unable to open Audio CD TOC file: "
                            + TOCfile.name());
        return false;
    }

    if (!TOC.setContent(&TOCfile))
    {
        VERBOSE(VB_GENERAL, "Unable to parse Audio CD TOC file: "
                            + TOCfile.name());
        TOCfile.close();
        return false;
    }

    m_tracks.clear();

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

            for (int i = m_firstTrack; i <= m_lastTrack; ++i)
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
    for (int i = 0; i <= m_lastTrack - m_firstTrack; ++i)
        checkSum += addDecimalDigits(m_tracks[i] / 75);

    m_diskID = ((checkSum % 255) << 24) | (int)m_lengthInSecs << 8
                                        | (m_lastTrack - m_firstTrack + 1);

    VERBOSE(VB_MEDIA, QString("CD %1, ID=%2")
                      .arg(devicename).arg(m_diskID, 8, 16));


    // First erase any existing metadata:
    for (unsigned int i = 0; i < m_mData.size(); ++i)
        delete m_mData[i];
    m_mData.clear();

    // Now, we need to:
    // 1) Lookup CDDB/FreeDB and get matching disks
    // 2) Let user select correct disk (possibly none of the looked up)
    // 3) Store Metadata for chosen disk


    // I haven't coded this up yet, so for now, just generate fake records
    for (unsigned int i = 0; i <= m_tracks.size(); ++i)
        m_mData.push_back(new Metadata("", "", "", "", "", "", 0, i, 1));



    inited = true;
    return true;
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
    if (!inited)
        initialize();

    return m_lastTrack;
}

int CdDecoder::getNumCDAudioTracks(void)
{
    if (!inited)
        initialize();

    return m_lastTrack - m_firstTrack + 1;
}

Metadata* CdDecoder::getMetadata(int track)
{
    if (!inited)
        initialize();

    return m_mData[track];
}

Metadata *CdDecoder::getLastMetadata()
{
    return NULL;
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
    static QString ext(".cda");
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
