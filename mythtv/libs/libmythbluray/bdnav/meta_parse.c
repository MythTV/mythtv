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

#include "file/file.h"
#include "util/bits.h"
#include "util/logging.h"
#include "util/macro.h"
#include "util/strutl.h"
#include "meta_parse.h"
#include "register.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#endif

#ifdef HAVE_LIBXML2
static void _parseManifestNode(xmlNode * a_node, META_DL *disclib)
{
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (xmlStrEqual(cur_node->parent->name, xmlCharStrdup("title"))) {
                if (xmlStrEqual(cur_node->name, xmlCharStrdup("name"))) {
                    disclib->di_name = (char*)xmlStrdup(xmlNodeGetContent(cur_node));
                }
                if (xmlStrEqual(cur_node->name, xmlCharStrdup("alternative"))) {
                    disclib->di_alternative = (char*)xmlStrdup(xmlNodeGetContent(cur_node));
                }
                if (xmlStrEqual(cur_node->name, xmlCharStrdup("numSets"))) {
                    disclib->di_num_sets = atoi((char*)xmlNodeGetContent(cur_node));
                }
                if (xmlStrEqual(cur_node->name, xmlCharStrdup("setNumber"))) {
                    disclib->di_set_number = atoi((char*)xmlNodeGetContent(cur_node));
                }
            }
            else if (xmlStrEqual(cur_node->parent->name, xmlCharStrdup("tableOfContents"))) {
                if (xmlStrEqual(cur_node->name, xmlCharStrdup("titleName")) && xmlGetProp(cur_node, xmlCharStrdup("titleNumber"))) {
                    int i = disclib->toc_count;
                    disclib->toc_count++;
                    disclib->toc_entries = realloc(disclib->toc_entries, (disclib->toc_count*sizeof(META_TITLE)));
                    disclib->toc_entries[i].title_number = atoi((const char*)xmlGetProp(cur_node, xmlCharStrdup("titleNumber")));
                    disclib->toc_entries[i].title_name = (char*)xmlStrdup(xmlNodeGetContent(cur_node));
                }
            }
            else if (xmlStrEqual(cur_node->parent->name, xmlCharStrdup("description"))) {
                if (xmlStrEqual(cur_node->name, xmlCharStrdup("thumbnail")) && xmlGetProp(cur_node, xmlCharStrdup("href"))) {
                    uint8_t i = disclib->thumb_count;
                    disclib->thumb_count++;
                    disclib->thumbnails = realloc(disclib->thumbnails, (disclib->thumb_count*sizeof(META_THUMBNAIL)));
                    int len = xmlStrlen(xmlGetProp(cur_node, xmlCharStrdup("href")));
                    disclib->thumbnails[i].path = (char*) malloc(len+1);
                    strcpy(disclib->thumbnails[i].path, (char*) xmlGetProp(cur_node, xmlCharStrdup("href")));
                    if (xmlGetProp(cur_node, xmlCharStrdup("size"))) {
                        sscanf((const char*)xmlGetProp(cur_node, xmlCharStrdup("size")), "%ix%i", &disclib->thumbnails[i].xres, &disclib->thumbnails[i].yres);
                    }
                    else {
                        disclib->thumbnails[i].xres = disclib->thumbnails[i].yres = -1;
                    }
                }
            }
        }
        _parseManifestNode(cur_node->children, disclib);
    }
}

static void _findMetaXMLfiles(META_ROOT *meta, const char *device_path)
{
    BD_DIR_H *dir;
    BD_DIRENT ent;
    char *path = NULL;
    path = str_printf("%s" DIR_SEP "BDMV" DIR_SEP "META" DIR_SEP "DL", device_path);
    dir = dir_open(path);
    if (dir == NULL) {
        DEBUG(DBG_DIR, "Failed to open meta dir %s\n", path);
        X_FREE(path);
        return;
    }
    int res;
    for (res = dir_read(dir, &ent); !res; res = dir_read(dir, &ent)) {
        if (ent.d_name[0] == '.')
            continue;
        else if (ent.d_name != NULL && strncasecmp(ent.d_name, "bdmt_", 5) == 0) {
            uint8_t i = meta->dl_count;
            meta->dl_count++;
            meta->dl_entries = realloc(meta->dl_entries, (meta->dl_count*sizeof(META_DL)));
            meta->dl_entries[i].filename = str_printf("%s", ent.d_name);
            strncpy(meta->dl_entries[i].language_code, ent.d_name+5,3);
            meta->dl_entries[i].language_code[3] = '\0';
        }
    }
    dir_close(dir);
    X_FREE(path);
}
#endif

META_ROOT *meta_parse(const char *device_path)
{
#ifdef HAVE_LIBXML2
    META_ROOT *root = calloc(1, sizeof(META_ROOT));
    root->dl_count = 0;

    xmlDocPtr doc;
    _findMetaXMLfiles(root, device_path);

    uint8_t i;
    for (i = 0; i < root->dl_count; i++) {
        char *base = NULL;
        base = str_printf("%s" DIR_SEP "BDMV" DIR_SEP "META" DIR_SEP "DL" , device_path);
        char *path = NULL;
        path = str_printf("%s" DIR_SEP "%s", base, root->dl_entries[i].filename);

        BD_FILE_H *handle = file_open(path, "rb");
        if (handle == NULL) {
            DEBUG(DBG_DIR, "Failed to open meta file (%s)\n", path);
            continue;
        }

        file_seek(handle, 0, SEEK_END);
        int64_t length = file_tell(handle);

        if (length > 0) {
            file_seek(handle, 0, SEEK_SET);
            uint8_t *data = malloc(length);
            int64_t size_read = file_read(handle, data, length);
            doc = xmlReadMemory((char*)data, size_read, base, NULL, 0);
            if (doc == NULL) {
                DEBUG(DBG_DIR, "Failed to parse %s\n", path);
                continue;
            }
            xmlNode *root_element = NULL;
            root_element = xmlDocGetRootElement(doc);
            root->dl_entries[i].di_name = root->dl_entries[i].di_alternative = NULL;
            root->dl_entries[i].di_num_sets = root->dl_entries[i].di_set_number = -1;
            root->dl_entries[i].toc_count = root->dl_entries[i].thumb_count = 0;
            root->dl_entries[i].toc_entries = NULL;
            root->dl_entries[i].thumbnails = NULL;
            _parseManifestNode(root_element, &root->dl_entries[i]);
            xmlFreeDoc(doc);
            X_FREE(data);
        }
        file_close(handle);
    }
    xmlCleanupParser();
    return root;
#else
    DEBUG(DBG_DIR, "configured without libxml2 - can't parse meta info\n");
    return NULL;
#endif
}

META_DL *meta_get(META_ROOT *meta_root, const char *language_code)
{
    unsigned i;

    if (meta_root == NULL || meta_root->dl_count == 0) {
        DEBUG(DBG_DIR, "meta_get not possible, no info available!\n");
        return NULL;
    }

    if (language_code) {
        for (i = 0; i < meta_root->dl_count; i++) {
            if (strcasecmp(language_code, meta_root->dl_entries[i].language_code) == 0) {
                return &meta_root->dl_entries[i];
            }
        }
        DEBUG(DBG_DIR, "requested disclib language '%s' not found\n", language_code);
    }

    for (i = 0; i < meta_root->dl_count; i++) {
        if (strcasecmp(DEFAULT_LANGUAGE, meta_root->dl_entries[i].language_code) == 0) {
            DEBUG(DBG_DIR, "using default disclib language '"DEFAULT_LANGUAGE"'\n");
            return &meta_root->dl_entries[i];
        }
    }

    DEBUG(DBG_DIR, "requested disclib language '%s' or default '"DEFAULT_LANGUAGE"' not found, using '%s' instead\n", language_code, meta_root->dl_entries[0].language_code);
    return &meta_root->dl_entries[0];
}

void meta_free(META_ROOT **p)
{
    if (p && *p)
    {
        uint8_t i;
        for (i = 0; i < (*p)->dl_count; i++) {
            uint32_t t;
            for (t=0; i < (*p)->dl_entries[i].toc_count; t++) {
                X_FREE((*p)->dl_entries[i].toc_entries[t].title_name);
                X_FREE((*p)->dl_entries[i].toc_entries);
            }
            for (t = 0; i < (*p)->dl_entries[i].thumb_count; t++) {
                X_FREE((*p)->dl_entries[i].thumbnails[t].path);
                X_FREE((*p)->dl_entries[i].thumbnails);
            }
            X_FREE((*p)->dl_entries[i].filename);
            X_FREE((*p)->dl_entries[i].di_name);
            X_FREE((*p)->dl_entries[i].di_alternative);
        }
        X_FREE(*p);
    }
}
