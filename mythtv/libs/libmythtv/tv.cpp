#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

extern int RecordVideo(int argc, char *argv[]);
extern int PlayVideo(int argc, char *argv[]);

int encoding;

struct args
{
    int argc;
    char **argv;
};

void *SpawnEncode(void *param)
{
  int i;
  struct args *TheArgs = (struct args *)param;

  for (i = 0; i < TheArgs->argc; i++) printf("enc %d %s\n", i, TheArgs->argv[i]);
  RecordVideo(TheArgs->argc, TheArgs->argv);

  return NULL;
}

void *SpawnDecode(void *param)
{
  int i;
  struct args *TheArgs = (struct args *)param;

  for (i = 0; i < TheArgs->argc; i++) printf("dec %d %s\n", i, TheArgs->argv[i]);

  while (!encoding)
      usleep(50);

  PlayVideo(TheArgs->argc, TheArgs->argv);

  encoding = 0;
  return NULL;
}

int main(int argc, char *argv[])
{
  struct args EncodeArgs;
  struct args DecodeArgs;
  pthread_t encode, decode;

  EncodeArgs.argc = argc;
  EncodeArgs.argv = argv;

  DecodeArgs.argc = 2;
  DecodeArgs.argv = malloc(sizeof(char *) * 3);
  DecodeArgs.argv[0] = strdup(argv[0]);
  DecodeArgs.argv[1] = malloc(strlen(argv[argc - 1]) + 10);
  strcpy(DecodeArgs.argv[1], argv[argc-1]);
  strcat(DecodeArgs.argv[1], ".nuv");
  DecodeArgs.argv[2] = NULL;
 
  encoding = 0;
  
  pthread_create(&encode, NULL, SpawnEncode, &EncodeArgs);
  pthread_create(&decode, NULL, SpawnDecode, &DecodeArgs);

  pthread_join(encode, NULL);
  pthread_join(decode, NULL);

  return 0;
}
