#include <qmap.h>

#define MARK_CUT_START    1
#define MARK_CUT_END      2
#define MARK_BLANK_FRAME  3
#define MARK_COMM_START   4
#define MARK_COMM_END     5

bool CheckFrameIsBlank(unsigned char *buf, int width, int height);
int GetFrameAvgBrightness(unsigned char *buf, int width, int height);
void BuildCommListFromBlanks(QMap<long long, int> &blanks, double fps,
        QMap<long long, int> &commMap);
void MergeCommList(QMap<long long, int> &commMap, double fps,
        QMap<long long, int> &commBreakMap);
void DeleteCommAtFrame(QMap<long long, int> &commMap, long long frame);

