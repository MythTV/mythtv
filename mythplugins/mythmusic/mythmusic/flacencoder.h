#ifndef FLACENCODER_H_
#define FLACENCODER_H_

#include <qstring.h>

class QProgressBar;
class Metadata;

void FlacEncode(const QString &infile, const QString &outfile, int qualitylevel,
                Metadata *metadata, int totalbytes, QProgressBar *progressbar);

#endif
