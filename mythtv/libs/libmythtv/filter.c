/*
 *  filter.c
 */

#include <dlfcn.h>
#include <string.h>
#include <stdio.h>

#include "filter.h"

static int filter_debug = 0;

VideoFilter *load_videoFilter(char *filter_name, char *options)
{  
    char module[256];
    void *handle;
    char *error;

    VideoFilter *filter = NULL;
    VideoFilter * (*initFilter)(char *options);
    snprintf(module, sizeof(module),"%s/lib%s.so", MYTHTV_FILTER_PATH, 
             filter_name);
    module[sizeof(module) - 1] = '\0';

    handle = dlopen(module, RTLD_NOW); 

    if (!handle) 
    {
        fprintf(stderr, "loading filter module %s failed\n", module); 
        if ((error = dlerror()) != NULL) 
        fprintf(stderr,"error:%s\n",error);
        return NULL;
    } 
    else 
    {
        if (filter_debug) 
            fprintf(stderr, "loading filter module %s\n", module); 
    }
  
  
    initFilter = (VideoFilter * (*)())dlsym(handle, "new_filter");   

    if ((error = dlerror()) != NULL)  
    {
        fprintf(stderr,"%s",error);
        return NULL;
    }
  
    filter = (*initFilter)(options);
    if (filter)
        filter->handle = handle;
  
    return filter;
}

int process_video_filters(VideoFrame *frame, VideoFilter **filters,
                          int numberFilters)
{
    int i;
    for (i = 0; i < numberFilters; i++) 
    {
        if (filters[i]->filter(filters[i], frame) < 0)
            fprintf(stderr," (%s) filter plugin '%s' returned error - "
                    "ignored\n", __FILE__, filters[i]->name);
    }

    return 0;
}

void filters_cleanup(VideoFilter **filters,int numberFilters)
{
    int i;
    for (i = 0; i < numberFilters; i++) 
    { 
        void *handle = filters[i]->handle;

        if (filter_debug)
            fprintf(stderr,"cleaning filter:%s\n",filters[i]->name);

        filters[i]->cleanup(filters[i]);
        dlclose(handle);
    }

    return;
}

