
#define DVB_DEV_FRONTEND    1
#define DVB_DEV_DVR         2
#define DVB_DEV_DEMUX       3
#define DVB_DEV_CA          4
#define DVB_DEV_AUDIO       5
#define DVB_DEV_VIDEO       6

#ifdef __cplusplus
extern "C" {
#endif

const char* dvbdevice(int type, int cardnum);

#ifdef __cplusplus
}
#endif
