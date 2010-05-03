#if !defined(_NAVIGATION_H_)
#define _NAVIGATION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "mpls_parse.h"
#include "clpi_parse.h"

#define CONNECT_NON_SEAMLESS 0
#define CONNECT_SEAMLESS 1

#define TITLES_ALL              0
#define TITLES_FILTER_DUP_TITLE 0x01
#define TITLES_FILTER_DUP_CLIP  0x02
#define TITLES_RELEVANT         (TITLES_FILTER_DUP_TITLE | TITLES_FILTER_DUP_CLIP)

typedef struct nav_title_s NAV_TITLE;

typedef struct {
    int      number;
    int      clip_ref;
    uint32_t clip_pkt;

    // Title relative metrics
    uint32_t title_pkt;
    uint32_t title_time;
    uint32_t duration;

    MPLS_PLM *plm;
} NAV_CHAP;

typedef struct {
    int      count;
    NAV_CHAP *chapter;
} NAV_CHAP_LIST;

typedef struct {
    char     name[11];
    uint32_t clip_id;
    int      ref;
    uint32_t pos;
    uint32_t start_pkt;
    uint32_t end_pkt;
    uint8_t  connection;
    uint8_t  angle;

    uint32_t start_time;
    uint32_t duration;

    uint32_t in_time;
    uint32_t out_time;

    // Title relative metrics
    uint32_t title_pkt;
    uint32_t title_time;

    NAV_TITLE *title;

    CLPI_CL  *cl;
} NAV_CLIP;

typedef struct {
    int      count;
    NAV_CLIP *clip;
} NAV_CLIP_LIST;

struct nav_title_s {
    char          *root;
    char          name[11];
    uint8_t       angle_count;
    uint8_t       angle;
    NAV_CLIP_LIST clip_list;
    NAV_CHAP_LIST chap_list;

    uint32_t      packets;
    uint32_t      duration;

    MPLS_PL       *pl;
};

typedef struct {
    char            name[11];
    uint32_t        mpls_id;
    uint32_t        duration;
    int             ref;
} NAV_TITLE_INFO;

typedef struct {
    int             count;
    NAV_TITLE_INFO  *title_info;
} NAV_TITLE_LIST;

char* nav_find_main_title(char *root);
NAV_TITLE* nav_title_open(char *root, char *playlist);
void nav_title_close(NAV_TITLE *title);
NAV_CLIP* nav_next_clip(NAV_TITLE *title, NAV_CLIP *clip);
NAV_CLIP* nav_packet_search(NAV_TITLE *title, uint32_t pkt, uint32_t *clip_pkt, uint32_t *out_pkt, uint32_t *out_time);
NAV_CLIP* nav_time_search(NAV_TITLE *title, uint32_t tick, uint32_t *clip_pkt, uint32_t *out_pkt);
void nav_clip_time_search(NAV_CLIP *clip, uint32_t tick, uint32_t *clip_pkt, uint32_t *out_pkt);
NAV_CLIP* nav_chapter_search(NAV_TITLE *title, int chapter, uint32_t *out_pkt);
uint32_t nav_angle_change_search(NAV_CLIP *clip, uint32_t pkt, uint32_t *time);
NAV_CLIP* nav_set_angle(NAV_TITLE *title, NAV_CLIP *clip, int angle);

NAV_TITLE_LIST* nav_get_title_list(char *root, uint32_t flags);
void nav_free_title_list(NAV_TITLE_LIST *title_list);

#ifdef __cplusplus
};
#endif

#endif // _NAVIGATION_H_
