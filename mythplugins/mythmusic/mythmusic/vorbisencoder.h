#ifndef VORBISENCODER_H_
#define VORBISENCODER_H_

#include <qstring.h>

class QProgressBar;
class Metadata;

void VorbisEncode(const QString &infile, const QString &outfile, 
                  int qualitylevel, Metadata *metadata, int totalbytes, 
                  QProgressBar *progressbar);

#endif
