#ifndef VORBISENCODER_H_
#define VORBISENCODER_H_

class QProgressBar;
class Metadata;
class MythContext;

void VorbisEncode(MythContext *context, const char *infile, const char *outfile,
                  int qualitylevel, Metadata *metadata, int totalbytes,
                  QProgressBar *progressbar);

#endif
