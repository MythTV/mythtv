// -*- Mode: c++ -*-
#ifndef SRTWRITER_H_
#define SRTWRITER_H_

#include <chrono>

// Qt
#include <QFile>
#include <QString>
#include <QTextStream>

// MythTV
#include "mythccextractorplayer.h"
#include "mythtvexp.h"

/**
 * Class to write SubRip files.
 */

class MTV_PUBLIC SRTWriter
{
  public:
    explicit SRTWriter(const QString &fileName);

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
