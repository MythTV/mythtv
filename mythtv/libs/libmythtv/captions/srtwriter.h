// -*- Mode: c++ -*-
#ifndef SRTWRITER_H_
#define SRTWRITER_H_

// Qt
#include <QHash>
#include <QImage>
#include <QPoint>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif
#include <QStringList>
#include <QTime>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

// MythTV
#include "captions/teletextextractorreader.h"
#include "decoders/avformatdecoder.h"
#include "format.h"
#include "mythccextractorplayer.h"
#include "mythplayer.h"

class OneSubtitle;

/**
 * Class to write SubRip files.
 */

class MTV_PUBLIC SRTWriter
{
  public:
    explicit SRTWriter(const QString &fileName) :
        m_outFile(fileName), m_outStream(&m_outFile)
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        m_outStream.setCodec("UTF-8");
#else
        m_outStream.setEncoding(QStringConverter::Utf8);
#endif
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
    static QString FormatTime(std::chrono::milliseconds time_in_msec);
    /// Output file.
    QFile m_outFile;
    /// Output stream associated with m_outFile.
    QTextStream m_outStream;
};

#endif /* SRTWRITER_H_ */
