#ifndef FLACENCODER_H_
#define FLACENCODER_H_

class QProgressBar;
class Metadata;
class MythContext;

void FlacEncode(MythContext *context, const char *infile, const char *outfile,
                int qualitylevel, Metadata *metadata, int totalbytes,
                QProgressBar *progressbar);

#endif
