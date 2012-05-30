// -*- Mode: c++ -*-
#ifndef SRTWRITER_H_
#define SRTWRITER_H_

#include <QStringList>
#include <QImage>
#include <QPoint>
#include <QHash>

#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

#include "avformatdecoder.h"
#include "mythplayer.h"
#include "teletextextractorreader.h"
#include "format.h"

#include "mythccextractorplayer.h"

class OneSubtitle;

/**
 * Class to write SubRip files.
 */

class MTV_PUBLIC SRTWriter
{
  public:
    SRTWriter(const QString &fileName) :
        m_outFile(fileName), m_outStream(&m_outFile), m_srtCounter(0)
    {
        m_outStream.setCodec("UTF-8");
        if (!m_outFile.open(QFile::WriteOnly))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to create '%1'")
                .arg(fileName));
        }
        else
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Created '%1'")
                .arg(fileName));                    
        }
    }
    ~SRTWriter(void)
    {
        m_outFile.close();
    }

    void AddSubtitle(const OneSubtitle &sub, int number);

    bool IsOpen(void) { return m_outFile.isOpen(); }
    void Flush(void) { m_outStream.flush(); }

  private:
    /// Formats time to format appropriate to SubRip file.
    static QString FormatTime(uint64_t time_in_msec);
    /// Output file.
    QFile m_outFile;
    /// Output stream associated with m_outFile.
    QTextStream m_outStream;
    /// Count of subtitles we already have written.
    int m_srtCounter;
};

#endif /* SRTWRITER_H_ */
