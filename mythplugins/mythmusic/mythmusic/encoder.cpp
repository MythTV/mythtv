#include <iostream>

#include <musicmetadata.h>
#include <mythcontext.h>

#include "encoder.h"


using namespace std;

Encoder::Encoder(const QString &outfile, int qualitylevel, MusicMetadata *metadata)
    : m_outfile(outfile), m_out(NULL), m_quality(qualitylevel),
      m_metadata(metadata)
{
    if (!m_outfile.isEmpty())
    {
        QByteArray outfile = m_outfile.toLocal8Bit();
        m_out = fopen(outfile.constData(), "w+");
        if (!m_out)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error opening output file: '%1'")
                    .arg(m_outfile));
        }
    }
}

Encoder::~Encoder()
{
    if (m_out)
        fclose(m_out);
}
