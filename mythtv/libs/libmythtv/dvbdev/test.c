#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "dvbdev.h"

int main()
{
#define OUT_SIZE 250000
#define IN_SIZE TS_SIZE
#define READ_SIZE TS_SIZE
#define NPIDS 2

  int pids[NPIDS] = {512, 650};
  unsigned char in_buf[IN_SIZE];
  unsigned char out_buf[OUT_SIZE];
  int result_len;
  ipack pv;
  ipack pa;

  init_ipack(&pv, IPACKS, ts_to_ps_write_out, 1);
  init_ipack(&pa, IPACKS, ts_to_ps_write_out, 1);

  while (read(STDIN_FILENO, in_buf, READ_SIZE) == READ_SIZE)
  {
    ts_to_ps(in_buf,
             512, 650,
             &pv, &pa,
             out_buf, OUT_SIZE,
             &result_len);
    fprintf(stderr, "got %d bytes from converter\n", result_len);
    if (result_len > 0)
        write(STDOUT_FILENO, out_buf, result_len);
  }
  return 0;
}
