#ifndef FLACENCODER_H_
#define FLACENCODER_H_

class QProgressBar;
class Metadata;

void FlacEncode(const char *infile, const char *outfile, int qualitylevel, 
                Metadata *metadata, int totalbytes, QProgressBar *progressbar);

#endif
