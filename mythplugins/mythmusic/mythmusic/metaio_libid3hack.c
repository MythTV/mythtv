#include <id3tag.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
  ID3_FILE_FLAG_ID3V1 = 0x0001
};

struct id3_file {
  FILE *iofile;
  enum id3_file_mode mode;
  char *path;

  int flags;

  struct id3_tag *primary;

  unsigned int ntags;
  struct filetag *tags;
};

struct filetag {
  struct id3_tag *tag;
  unsigned long location;
  id3_length_t length;
};


/*
 * NAME: v2_write()
 * DESCRIPTION:  write ID3v2 tag modifications to a file
 */
static
int myth_v2_write(struct id3_file *file,
                  id3_byte_t const *data, id3_length_t length)
{
  int result = 0;
  int file_size, overlap;
  char *buffer = NULL;
  int buffersize;
  
  int bytesread1, bytesread2, bytesread_tmp;
  char *ring1 = NULL, *ring2 = NULL, *ring_tmp = NULL;

  assert(!data || length > 0);

  if (!data
      || (!(file->ntags == 1 && !(file->flags & ID3_FILE_FLAG_ID3V1))
          && !(file->ntags == 2 &&  (file->flags & ID3_FILE_FLAG_ID3V1)))) {
    /* no v2 tag. we should create one */
    
    /* ... */
    
    goto done;
  }

  if (file->tags[0].length == length) {
    /* easy special case: rewrite existing tag in-place */

    if (fseek(file->iofile, file->tags[0].location, SEEK_SET) == -1 ||
        fwrite(data, length, 1, file->iofile) != 1 ||
        fflush(file->iofile) == EOF)
      return -1;

    goto done;
    
  }
  
  /* the new tag has a different size */
  
  /* calculate the difference in tag sizes.
   * we'll need at least double this difference to write the file again using 
   * optimal memory allocation, but avoiding a temporary file.
   */
  overlap = length - file->tags[0].length;
  
  if (overlap > 0) {
    buffersize = overlap*2;
    buffer = (char*)malloc(buffersize);
    if (!buffer)
      return -1;
      
    ring1 = buffer;
    ring2 = &buffer[overlap];
  } else {
    /* let's use a 100kB buffer */
    buffersize = 100*1024;
    buffer = (char*)malloc(buffersize);
  }
  
  /* find out the filesize */
  fseek(file->iofile, 0, SEEK_END);
  file_size = ftell(file->iofile);
  
  /* Seek to start of data */
  if (-1 == fseek(file->iofile, file->tags[0].location + file->tags[0].length,
                  SEEK_SET))
    goto fail;
  
  /* fill our buffer, if needed */
  if (overlap > 0) {
    if (1 != fread(buffer, buffersize, 1, file->iofile))
      goto fail;
  }
  
  /* write the tag where the old one was */
  if (-1 == fseek(file->iofile, file->tags[0].location, SEEK_SET)
      || 1 != fwrite(data, length, 1, file->iofile))
    goto fail;
  
  /* loop through reading and writing the data */
  if (overlap > 0) {
   
    /* File is getting larger */
    bytesread1 = bytesread2 = overlap;
    while (0 != bytesread1 || 0 != bytesread2) {
  
      /* Write the contents of ring1 */
      if (1 != fwrite(ring1, bytesread1, 1, file->iofile))
        goto fail;
      
      /* Read the next "overlap" bytes into ring1 */
      bytesread1 = fread(ring1, 1, overlap, file->iofile);
      
      if (bytesread1 > 0) {
        /* Seek back that many bytes */
        if (-1 == fseek(file->iofile, -1*bytesread1, SEEK_CUR))
          goto fail;
      }
      
      /* swap rings */
      ring_tmp = ring1;
      ring1 = ring2;
      ring2 = ring_tmp;
      
      bytesread_tmp = bytesread1;
      bytesread1 = bytesread2;
      bytesread2 = bytesread_tmp;
    }
  } else {
     
    /* File is getting smaller */
    
    /* remember "overlap" is negative so let's absolute it */
    overlap *= -1;
    
    while (0 == feof(file->iofile)) {
      
      /* seek ahead "overlap" bytes */
      if (-1 == fseek(file->iofile, overlap, SEEK_CUR))
        goto fail;
      
      /* read buffer */
      bytesread1 = fread(buffer, 1, buffersize, file->iofile);
      
      if (bytesread1 > 0) {
        /* seek back that many bytes */
        if (-1 == fseek(file->iofile, -1*(bytesread1+overlap), SEEK_CUR))
          goto fail;
      
        /* write the buffer contents */
        if (1 != fwrite(buffer, bytesread1, 1, file->iofile))
          goto fail;
      } else {
        /* just seek back to the point we should be at for truncation */
        if (-1 == fseek(file->iofile, -1*overlap, SEEK_CUR))
          goto fail;
      }
      
      /* Check to see if we've reached the end of the file */
      if (bytesread1 != buffersize)
        break;
    }
  }
  
  if (buffer)
  {
    free(buffer);
    buffer = NULL;
  }
  
  /* flush the FILE */
  if (fflush(file->iofile) == EOF)
    goto fail;
  
  /* truncate if required */
  if (ftell(file->iofile) < file_size)
    ftruncate(fileno(file->iofile), ftell(file->iofile));
     
  if (0) {
  fail:
    if (buffer) free(buffer);
    result = -1;
  }

 done:
  return result;
}

/*
 * NAME: file->update()
 * DESCRIPTION:  rewrite tag(s) to a file
 */
int myth_id3_file_update(struct id3_file *file)
{
  int options, result = 0;
  id3_length_t v2size = 0;
  id3_byte_t *id3v2 = 0;

  assert(file);

  if (file->mode != ID3_FILE_MODE_READWRITE)
    return -1;

  options = id3_tag_options(file->primary, 0, 0);

  /* render ID3v2 */

  id3_tag_options(file->primary, ID3_TAG_OPTION_ID3V1, 0);

  v2size = id3_tag_render(file->primary, 0);
  if (v2size) {
    id3v2 = malloc(v2size);
    if (id3v2 == 0)
      goto fail;

    v2size = id3_tag_render(file->primary, id3v2);
    if (v2size == 0) {
      free(id3v2);
      id3v2 = 0;
    }
  }

  /* write tags */

  if (myth_v2_write(file, id3v2, v2size) == -1)
    goto fail;

  rewind(file->iofile);

  /* update file tags array? ... */

  if (0) {
  fail:
    result = -1;
  }

  /* clean up; restore tag options */

  if (id3v2)
    free(id3v2);

  id3_tag_options(file->primary, ~0, options);

  return result;
}

