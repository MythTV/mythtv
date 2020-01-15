#ifndef AUDIOPINK
#define AUDIOPINK

#include "mythexp.h"

#define PINK_MAX_RANDOM_ROWS   (30)
#define PINK_RANDOM_BITS       (24)
#define PINK_RANDOM_SHIFT      ((sizeof(int32_t)*8)-PINK_RANDOM_BITS)

// NOLINTNEXTLINE(modernize-use-using)
typedef struct
{
  int32_t   pink_rows[PINK_MAX_RANDOM_ROWS];
  int32_t   pink_running_sum;  /* Used to optimize summing of generators. */
  int32_t   pink_index;        /* Incremented each sample. */
  int32_t   pink_index_mask;   /* Index wrapped by ANDing with this mask. */
  float     pink_scalar;       /* Used to scale within range of -1.0 to +1.0 */
} pink_noise_t;

void initialize_pink_noise( pink_noise_t *pink, int num_rows );
float generate_pink_noise_sample( pink_noise_t *pink );

#endif
