#ifndef AUDIOPINK
#define AUDIOPINK

#include <array>
#include "libmyth/mythexp.h"

static constexpr int8_t PINK_DEFAULT_ROWS    { 12 };
static constexpr int8_t PINK_MAX_RANDOM_ROWS { 32 };
static constexpr int8_t PINK_RANDOM_BITS     { 24 };
static constexpr int8_t PINK_RANDOM_SHIFT    { (sizeof(int32_t)*8)-PINK_RANDOM_BITS };

struct pink_noise_t
{
  std::array<int32_t,PINK_MAX_RANDOM_ROWS> pink_rows;
  int32_t   pink_running_sum;  /* Used to optimize summing of generators. */
  int32_t   pink_index;        /* Incremented each sample. */
  int32_t   pink_index_mask;   /* Index wrapped by ANDing with this mask. */
  float     pink_scalar;       /* Used to scale within range of -1.0 to +1.0 */
};

MPUBLIC void initialize_pink_noise( pink_noise_t *pink, int num_rows = PINK_DEFAULT_ROWS);
MPUBLIC float generate_pink_noise_sample( pink_noise_t *pink );

#endif
