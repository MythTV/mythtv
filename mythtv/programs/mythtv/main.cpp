#include "tv.h"

int main(int argc, char *argv[])
{
    string startChannel = "3";

    TV *tv = new TV(startChannel);
    tv->LiveTV();

    //tv->StartRecording(startChannel, 60, "/mnt/store/test.nuv");

    //tv->Playback("/mnt/store/test.nuv");
}


