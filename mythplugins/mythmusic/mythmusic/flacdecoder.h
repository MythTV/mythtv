#ifndef FLACDECODER_H_
#define FLACDECODER_H_

#define HAVE_INTTYPES_H
#include <FLAC/all.h>
#include <FLAC/export.h>
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
  /* FLAC 1.0.4 to 1.1.2 */
  #define StreamDecoderReadStatus FLAC__SeekableStreamDecoderReadStatus
  #define StreamDecoder FLAC__SeekableStreamDecoder
  #define STREAM_DECODER_READ_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR
  #define STREAM_DECODER_READ_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK
  #define StreamDecoderSeekStatus FLAC__SeekableStreamDecoderSeekStatus
  #define STREAM_DECODER_SEEK_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR
  #define STREAM_DECODER_SEEK_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK
  #define StreamDecoderTellStatus FLAC__SeekableStreamDecoderTellStatus
  #define STREAM_DECODER_TELL_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK
  #define StreamDecoderLengthStatus FLAC__SeekableStreamDecoderLengthStatus
  #define STREAM_DECODER_LENGTH_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK
  #define FileDecoder FLAC__FileDecoder
  #define decoder_new() FLAC__seekable_stream_decoder_new()
  #define decoder_set_md5_checking(dec, op) \
          FLAC__seekable_stream_decoder_set_md5_checking(dec, op)
  #define decoder_setup(dec, read, seek, tell, length, \
                        eof, write, metadata, error, data) \
          { \
            FLAC__seekable_stream_decoder_set_read_callback(dec, read); \
            FLAC__seekable_stream_decoder_set_seek_callback(dec, seek); \
            FLAC__seekable_stream_decoder_set_tell_callback(dec, tell); \
            FLAC__seekable_stream_decoder_set_length_callback(dec, length); \
            FLAC__seekable_stream_decoder_set_eof_callback(dec, eof); \
            FLAC__seekable_stream_decoder_set_write_callback(dec, write); \
            FLAC__seekable_stream_decoder_set_metadata_callback(dec, metadata); \
            FLAC__seekable_stream_decoder_set_error_callback(dec, error); \
            FLAC__seekable_stream_decoder_set_client_data(dec, data); \
          }
  #define decoder_process_metadata FLAC__seekable_stream_decoder_process_until_end_of_metadata
  #define decoder_finish FLAC__seekable_stream_decoder_finish
  #define decoder_delete FLAC__seekable_stream_decoder_delete
  #define DecoderState FLAC__SeekableStreamDecoderState
  #define decoder_seek_absolute FLAC__seekable_stream_decoder_seek_absolute
  #define decoder_process_single FLAC__seekable_stream_decoder_process_single
  #define decoder_get_state FLAC__seekable_stream_decoder_get_state
  #define bytesSize unsigned
#else
  /* FLAC 1.1.3 and up */
  #define NEWFLAC
  #define StreamDecoderReadStatus FLAC__StreamDecoderReadStatus
  #define StreamDecoder FLAC__StreamDecoder
  #define STREAM_DECODER_READ_STATUS_ERROR FLAC__STREAM_DECODER_READ_STATUS_ABORT
  #define STREAM_DECODER_READ_STATUS_OK FLAC__STREAM_DECODER_READ_STATUS_CONTINUE
  #define StreamDecoderSeekStatus FLAC__StreamDecoderSeekStatus
  #define STREAM_DECODER_SEEK_STATUS_ERROR FLAC__STREAM_DECODER_SEEK_STATUS_ERROR
  #define STREAM_DECODER_SEEK_STATUS_OK FLAC__STREAM_DECODER_SEEK_STATUS_OK
  #define StreamDecoderTellStatus FLAC__StreamDecoderTellStatus
  #define STREAM_DECODER_TELL_STATUS_OK FLAC__STREAM_DECODER_TELL_STATUS_OK
  #define StreamDecoderLengthStatus FLAC__StreamDecoderLengthStatus
  #define STREAM_DECODER_LENGTH_STATUS_OK FLAC__STREAM_DECODER_LENGTH_STATUS_OK
  #define FileDecoder FLAC__StreamDecoder
  #define decoder_new() FLAC__stream_decoder_new()
  #define decoder_set_md5_checking(dec, op) \
          FLAC__stream_decoder_set_md5_checking(dec, op)
  #define decoder_setup(decoder, read, seek, tell, length, eof, write, metadata, error, data) \
          FLAC__stream_decoder_init_stream(decoder, read, seek, tell, length, \
                                           eof, write, metadata, error, data)
  #define decoder_process_metadata FLAC__stream_decoder_process_until_end_of_metadata
  #define decoder_finish FLAC__stream_decoder_finish
  #define decoder_delete FLAC__stream_decoder_delete
  #define DecoderState FLAC__StreamDecoderState
  #define decoder_seek_absolute FLAC__stream_decoder_seek_absolute
  #define decoder_process_single FLAC__stream_decoder_process_single
  #define decoder_get_state FLAC__stream_decoder_get_state
  #define bytesSize size_t
#endif

#include "decoder.h"

class Metadata;

class FlacDecoder : public Decoder
{
  public:
    FlacDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~FlacDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

    void doWrite(const FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
    void setFlacMetadata(const FLAC__StreamMetadata *metadata);

    MetaIO *doCreateTagger(void);

  private:
    void run();

    void flush(bool = FALSE);
    void deinit();

    bool inited, user_stop;
    int stat;
    char *output_buf;
    ulong output_bytes, output_at;

    StreamDecoder *decoder;

    unsigned int bks;
    bool done, finish;
    long len, freq, bitrate;
    int chan;
    int bitspersample;
    double totalTime, seekTime;
    unsigned long totalsamples;


};

#endif

