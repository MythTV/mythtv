#include <unistd.h>
#include "tv.h"

int main(int argc, char *argv[])
{
    string startChannel = "3";

    TV *tv = new TV(startChannel);
    tv->LiveTV();

    //tv->StartRecording(startChannel, 60, "/mnt/store/test.nuv");
    //tv->Playback("/mnt/store/test.nuv");

    while (tv->GetState() == kState_None)
        usleep(1000);

    while (tv->GetState() != kState_None)
        usleep(1000);

    sleep(1);
    printf("shutting down\n");
    delete tv;
    printf("Goodbye\n");

    exit(0);
}
