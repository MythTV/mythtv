#ifndef VORBISENCODER_H_
#define VORBISENCODER_H_

class QProgressBar;
class Metadata;

void VorbisEncode(const char *infile, const char *outfile, int qualitylevel, 
                  Metadata *metadata, int totalbytes, 
                  QProgressBar *progressbar);

#endif
