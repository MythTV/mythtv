#ifndef METAIO_LIBID3HACK_H_
#define METAIO_LIBID3HACK_H_

extern "C"
{
#define ID3_FILE_UPDATE myth_id3_file_update

int myth_id3_file_update(struct id3_file *file);
}
#endif
