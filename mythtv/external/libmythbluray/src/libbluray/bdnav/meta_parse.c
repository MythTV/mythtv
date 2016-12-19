/*
 * This file is part of libbluray
 * Copyright (C) 2010 fraxinas
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "meta_parse.h"

#include "meta_data.h"

#include "disc/disc.h"

#include "file/file.h"
#include "util/bits.h"
#include "util/logging.h"
#include "util/macro.h"
#include "util/strutl.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#endif

#define DEFAULT_LANGUAGE  "eng"


#define BAD_CAST_CONST (const xmlChar *)
#define XML_FREE(p) (xmlFree(p), p = NULL)

#define MAX_META_FILE_SIZE  0xfffff

#ifdef HAVE_LIBXML2
static void _parseManifestNode(xmlNode * a_node, META_DL *disclib)
{
    xmlNode *cur_node = NULL;
    xmlChar *tmp;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrEqual(cur_node->parent->name, BAD_CAST_CONST "title")) {
                if (xmlStrEqual(cur_node->name, BAD_CAST_CONST "name")) {
                    disclib->di_name = (char*)xmlNodeGetContent(cur_node);
                }
                if (xmlStrEqual(cur_node->name, BAD_CAST_CONST "alternative")) {
                    disclib->di_alternative = (char*)xmlNodeGetContent(cur_node);
                }
                if (xmlStrEqual(cur_node->name, BAD_CAST_CONST "numSets")) {
                    disclib->di_num_sets = atoi((char*)(tmp = xmlNodeGetContent(cur_node)));
                    XML_FREE(tmp);
                }
                if (xmlStrEqual(cur_node->name, BAD_CAST_CONST "setNumber")) {
                    disclib->di_set_number = atoi((char*)(tmp = xmlNodeGetContent(cur_node)));
                    XML_FREE(tmp);
                }
            }
            else if (xmlStrEqual(cur_node->parent->name, BAD_CAST_CONST "tableOfContents")) {
                if (xmlStrEqual(cur_node->name, BAD_CAST_CONST "titleName") && (tmp = xmlGetProp(cur_node, BAD_CAST_CONST "titleNumber"))) {
                    META_TITLE *new_entries = realloc(disclib->toc_entries, ((disclib->toc_count + 1)*sizeof(META_TITLE)));
                    if (new_entries) {
                        int i = disclib->toc_count;
                        disclib->toc_count++;
                        disclib->toc_entries = new_entries;
                        disclib->toc_entries[i].title_number = atoi((const char*)tmp);
                        disclib->toc_entries[i].title_name = (char*)xmlNodeGetContent(cur_node);
                    }
                    XML_FREE(tmp);
                }
            }
            else if (xmlStrEqual(cur_node->parent->name, BAD_CAST_CONST "description")) {
                if (xmlStrEqual(cur_node->name, BAD_CAST_CONST "thumbnail") && (tmp = xmlGetProp(cur_node, BAD_CAST_CONST "href"))) {
                    META_THUMBNAIL *new_thumbnails = realloc(disclib->thumbnails, ((disclib->thumb_count + 1)*sizeof(META_THUMBNAIL)));
                    if (new_thumbnails) {
                        uint8_t i = disclib->thumb_count;
                        disclib->thumb_count++;
                        disclib->thumbnails = new_thumbnails;
                        disclib->thumbnails[i].path = (char *)tmp;
                        if ((tmp = xmlGetProp(cur_node, BAD_CAST_CONST "size"))) {
                            int x = 0, y = 0;
                            sscanf((const char*)tmp, "%ix%i", &x, &y);
                            disclib->thumbnails[i].xres = x;
                            disclib->thumbnails[i].yres = y;
                            XML_FREE(tmp);
                        }
                        else {
                          disclib->thumbnails[i].xres = disclib->thumbnails[i].yres = -1;
                        }
                    }
                }
            }
        }
        _parseManifestNode(cur_node->children, disclib);
    }
}

static void _findMetaXMLfiles(META_ROOT *meta, BD_DISC *disc)
{
    BD_DIR_H *dir;
    BD_DIRENT ent;
    dir = disc_open_dir(disc, "BDMV" DIR_SEP "META" DIR_SEP "DL");
    if (dir == NULL) {
        BD_DEBUG(DBG_DIR, "Failed to open meta dir BDMV/META/DL/\n");
        return;
    }
    int res;
    for (res = dir_read(dir, &ent); !res; res = dir_read(dir, &ent)) {
        if (ent.d_name[0] == '.')
            continue;
        else if (strncasecmp(ent.d_name, "bdmt_", 5) == 0) {
            META_DL *new_dl_entries = realloc(meta->dl_entries, ((meta->dl_count + 1)*sizeof(META_DL)));
            if (new_dl_entries) {
                uint8_t i = meta->dl_count;
                meta->dl_count++;
                meta->dl_entries = new_dl_entries;
                memset(&meta->dl_entries[i], 0, sizeof(meta->dl_entries[i]));

                meta->dl_entries[i].filename = str_dup(ent.d_name);
                strncpy(meta->dl_entries[i].language_code, ent.d_name+5,3);
                meta->dl_entries[i].language_code[3] = '\0';
                str_tolower(meta->dl_entries[i].language_code);
            }
        }
    }
    dir_close(dir);
}
#endif

META_ROOT *meta_parse(BD_DISC *disc)
{
#ifdef HAVE_LIBXML2
    META_ROOT *root = calloc(1, sizeof(META_ROOT));
    if (!root) {
        BD_DEBUG(DBG_CRIT, "out of memory\n");
        return NULL;
    }
    root->dl_count = 0;

    xmlDocPtr doc;
    _findMetaXMLfiles(root, disc);

    uint8_t i;
    for (i = 0; i < root->dl_count; i++) {
        uint8_t *data = NULL;
        size_t size;
        size = disc_read_file(disc, "BDMV" DIR_SEP "META" DIR_SEP "DL",
                              root->dl_entries[i].filename,
                              &data);
        if (!data || size == 0) {
            BD_DEBUG(DBG_DIR, "Failed to read BDMV/META/DL/%s\n", root->dl_entries[i].filename);
        } else {
                doc = xmlReadMemory((char*)data, (int)size, NULL, NULL, 0);
                if (doc == NULL) {
                    BD_DEBUG(DBG_DIR, "Failed to parse BDMV/META/DL/%s\n", root->dl_entries[i].filename);
                } else {
                    xmlNode *root_element = NULL;
                    root_element = xmlDocGetRootElement(doc);
                    root->dl_entries[i].di_name = root->dl_entries[i].di_alternative = NULL;
                    root->dl_entries[i].di_num_sets = root->dl_entries[i].di_set_number = -1;
                    root->dl_entries[i].toc_count = root->dl_entries[i].thumb_count = 0;
                    root->dl_entries[i].toc_entries = NULL;
                    root->dl_entries[i].thumbnails = NULL;
                    _parseManifestNode(root_element, &root->dl_entries[i]);
                    xmlFreeDoc(doc);
                }
            X_FREE(data);
        }
    }
    xmlCleanupParser();
    return root;
#else
    (void)disc;
    BD_DEBUG(DBG_DIR, "configured without libxml2 - can't parse meta info\n");
    return NULL;
#endif
}

const META_DL *meta_get(const META_ROOT *meta_root, const char *language_code)
{
#ifdef HAVE_LIBXML2
    unsigned i;

    if (meta_root == NULL || meta_root->dl_count == 0) {
        BD_DEBUG(DBG_DIR, "meta_get not possible, no info available!\n");
        return NULL;
    }

    if (language_code) {
        for (i = 0; i < meta_root->dl_count; i++) {
            if (strcmp(language_code, meta_root->dl_entries[i].language_code) == 0) {
                return &meta_root->dl_entries[i];
            }
        }
        BD_DEBUG(DBG_DIR, "requested disclib language '%s' not found\n", language_code);
    }

    for (i = 0; i < meta_root->dl_count; i++) {
        if (strcmp(DEFAULT_LANGUAGE, meta_root->dl_entries[i].language_code) == 0) {
            BD_DEBUG(DBG_DIR, "using default disclib language '"DEFAULT_LANGUAGE"'\n");
            return &meta_root->dl_entries[i];
        }
    }

    BD_DEBUG(DBG_DIR, "requested disclib language '%s' or default '"DEFAULT_LANGUAGE"' not found, using '%s' instead\n", language_code, meta_root->dl_entries[0].language_code);
    return &meta_root->dl_entries[0];
#else
    (void)meta_root;
    (void)language_code;
    return NULL;
#endif
}

void meta_free(META_ROOT **p)
{
    (void)p;
#ifdef HAVE_LIBXML2
    if (p && *p)
    {
        uint8_t i;
        for (i = 0; i < (*p)->dl_count; i++) {
            uint32_t t;
            for (t = 0; t < (*p)->dl_entries[i].toc_count; t++) {
                XML_FREE((*p)->dl_entries[i].toc_entries[t].title_name);
            }
            for (t = 0; t < (*p)->dl_entries[i].thumb_count; t++) {
                XML_FREE((*p)->dl_entries[i].thumbnails[t].path);
            }
            X_FREE((*p)->dl_entries[i].toc_entries);
            X_FREE((*p)->dl_entries[i].thumbnails);
            X_FREE((*p)->dl_entries[i].filename);
            XML_FREE((*p)->dl_entries[i].di_name);
            XML_FREE((*p)->dl_entries[i].di_alternative);
        }
        X_FREE((*p)->dl_entries);
        X_FREE(*p);
    }
#endif
}
