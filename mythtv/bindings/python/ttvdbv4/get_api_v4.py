# -*- coding: UTF-8 -*-

# ----------------------------------------------------
# Purpose:   MythTV Python Bindings for TheTVDB v4 API
# Copyright: (c) 2021 Roland Ernst
# License:   GPL v2 or later, see LICENSE for details
# ----------------------------------------------------

"""
Parse openapi specification for ttvdb v4.
See https://github.com/thetvdb/v4-api/blob/main/docs/swagger.yml
Create definitions and api for the files
     - definitions.py
     - ttvdbv4_api.py
"""

import sys
import os
import yaml
import re


# default strings for api functions
func_str = \
    "def {fname}({fparams}):\n"
param_str = \
    "    params = {}\n"
param_item_str = \
    "    if %s is not None:\n\
        params['%s'] = %s\n"
query_str = \
    "    res = _query_api(%s)\n"
query_str_params = \
    "    res = _query_api(%s, params)\n"
query_yielded_str = \
    "    return _query_yielded(%s, %s, params, %s)\n"
res_str = \
    "    data = res['data'] if res.get('data') is not None else None\n"
array_str = \
    "[%s(x) for x in %s] if data is not None else []"
item_str = \
    "%s(%s) if data is not None else None"


# default values for basic types
defaults = {'string':  "''",
            'integer': 0,
            'number':  0.0,
            'boolean': False
            }


# global
pathlist = []


def read_yaml_from_file(api_spec):
    with open(api_spec) as f:
        data = yaml.safe_load(f)
    return data


def print_api(title, version, pathlist, api_calls):
    print('"""Generated API for thetvdb.com {} v {}"""'.format(title, version))
    print("\n")
    for i in pathlist:
        print(i)
    print("\n\n")
    print(api_calls)


def print_api_gets(pathname, pathdata):
    global pathlist
    paged = False
    pstring = ""
    # get the function definition for 'operationId'
    if pathdata.get('get') is not None:
        operationId = pathdata['get']['operationId']
        plist = []       # for the 'path' string
        pitems = []      # for the function parameter string
        params = {}      # for the query parameters
        if pathdata['get'].get('parameters') is not None:
            # required params are member of the path
            # optional params are attached as 'params' dict
            for param in pathdata['get']['parameters']:
                # search for paged requests
                pkey = param.get('name').replace('-', '_')
                pvalue = pkey
                required = param.get('required', False)
                if pkey == 'page':
                    paged = True
                    continue
                if pkey == 'id':
                    pvalue = 'str(id)'
                if required:
                    plist.append("%s=%s" % (pkey, pvalue))
                    pitems.append(pkey)
                else:
                    params[pkey] = pvalue
                    pitems.append("%s=None" % pvalue)
            if paged:
                params['page'] = 'page'
                pitems.append("%s=%s" % ('page', 0))
                pitems.append("%s=%s" % ('yielded', False))
        pstring += func_str.format(fname=operationId,
                                   fparams=", ".join(pitems))

        # define the parameter
        if params:
            pstring += param_str
            for k, v in params.items():
                pstring += param_item_str % (k, k, v)

        # define the url
        path = "%s_path.format(%s)" % (operationId, ", ".join(plist))
        pstring += "    path = %s\n" % path

        # evaluate response properties['data']:
        content = pathdata['get']['responses']['200']['content']
        data = content['application/json']['schema']['properties']['data']

        # look for references ('$ref') starting with '#/components/schemas/'
        ref_str = '#/components/schemas/'
        tmplst = []
        tmpref = ""
        listname = "listname=None"
        if data.get('type') is not None:
            if data.get('items'):
                ref = data['items']['$ref']
                if ref.startswith(ref_str):
                    if data.get('type') == 'array':
                        tmpref = ref.split('/')[-1]
                        tmplst.append("%s" % (array_str % (tmpref, 'data')))
            elif data.get('properties'):
                for prop in data['properties'].keys():
                    if data['properties'][prop].get('$ref') is not None:
                        ref = data['properties'][prop]['$ref']
                        if ref.startswith(ref_str):
                            tmpref = ref.split('/')[-1]
                            tmplst.append(item_str % (tmpref, "data['%s']" % prop))
                    elif data['properties'][prop].get('items') is not None:
                        if data['properties'][prop].get('type') == 'array':
                            ref = data['properties'][prop]['items']['$ref']
                            if ref.startswith(ref_str):
                                listname = "listname='%s'" % prop
                                tmpref = ref.split('/')[-1]
                                tmplst.append(array_str % (tmpref, "data['%s']" % prop))

        elif data.get('$ref') is not None:
            if data['$ref'].startswith(ref_str):
                pref = data['$ref'].split('/')[-1]
                tmplst.append("%s" % (item_str % (pref, 'data')))

        # format output
        add_ident = ""
        if params:
            if paged:
                add_ident = "    "
                pstring += add_ident + "if yielded:\n"
                pstring += add_ident + query_yielded_str % (tmpref, "path", listname)
                pstring += add_ident + "else:\n"

            pstring += add_ident + query_str_params % ("path")
            pstring += add_ident + res_str
        else:
            pstring += query_str % ("path")
            pstring += res_str

        pstring += \
            '    %sreturn( %s )' % (add_ident,
                                    (",\n            " + add_ident).join(tmplst))

        # update pathlist as well
        # replace ('-', '_') in parameters identified within '{}'
        pattern = re.compile(r'{[a-z]+-[a-z]+}')
        s = '%s_path = TTVDBV4_path + "%s"' % (operationId, pathname)
        for match in pattern.findall(s):
            s = s.replace(match, match.replace('-', '_'))
        pathlist.append(s)

    return pstring


def print_definition(name, defitem, setdefaults=False):

    # string definitions
    defstr = '#/components/schemas'      # openapi 3.0
    classstr = \
        'class %s(object):\n' % name + \
        '    """%s"""\n' % defitem['description'] + \
        '    def __init__(self, data):\n'
    handle_list_str = \
        "        self.%s = _handle_list(%s, data.get('%s'))\n"
    get_list_str = \
        "        self.%s = _get_list(data, '%s')\n"
    handle_single_str = \
        "        self.%s = _handle_single(%s, data.get('%s'))\n"
    translations_str = \
        "        self.fetched_translations = []\n"
    similarity_str = \
        "        self.name_similarity = 0.0\n"
    added_str = \
        "        # additional attributes needed by the mythtv grabber script:\n"

    if defitem.get('properties') is None:
        classstr += "        pass\n"
    else:
        needs_translations = False
        for i in defitem['properties'].keys():
            if ('items') in defitem['properties'][i]:
                # handle arrays and lists of basic types
                if ('type') in defitem['properties'][i]:
                    if defitem['properties'][i]['type'] == 'array':
                        if ('$ref') in defitem['properties'][i]['items']:
                            ref = defitem['properties'][i]['items']['$ref']
                            atype = ref.split("/")[-1]
                            classstr += handle_list_str % (i, atype, i)
                        else:
                            if i == 'nameTranslations':
                                needs_translations = True
                            classstr += get_list_str % (i, i)

            elif ('$ref') in defitem['properties'][i]:
                # handle special types
                v = defitem['properties'][i]['$ref']
                stype = v.split("/")[-1]
                classstr += handle_single_str % (i, stype, i)

            elif ('type') in defitem['properties'][i]:
                # handle basic types
                stype = defitem['properties'][i]['type']
                if setdefaults:
                    d = defaults.get(stype)
                    s = "        self.%s = data.get('%s', %s)" % (i, i, d)
                else:
                    s = "        self.%s = data.get('%s')" % (i, i)
                alignment = 80 - len(s) + len(stype)
                classstr += s + ("# %s\n" % stype).rjust(alignment)
        if needs_translations:
            # below are additions needed by the mythtv grabber script
            classstr += added_str
            classstr += translations_str
            classstr += similarity_str

    return(classstr)


if __name__ == '__main__':
    """
    Download the latest api specification from the TheTVDB official repo:
    https://github.com/thetvdb/v4-api/blob/main/docs/swagger.yml
    """
    api_spec = sys.argv[1]
    if not os.path.isfile(api_spec):
        print("Error: main() needs to be called with an OAS3 spec file (yaml)")
        sys.exit(1)
    apidata = read_yaml_from_file(api_spec)
    api_title = apidata['info']['title']
    api_version = apidata['info']['version']
    apiv4_basepath = apidata['servers'][0]['url']

    pathlist.append('TTVDBV4_path = "{}"'.format(apiv4_basepath))

    # get api functions
    api_calls = ""
    for k in apidata['paths'].keys():
        if apidata['paths'][k].get('get'):
            api_calls += print_api_gets(k, apidata['paths'][k])
            api_calls += "\n\n\n"

    print_api(api_title, api_version, pathlist, api_calls)

    # get api definitions
    api_defs = ""
    api_defs += ('"""Generated API for thetvdb.com {} v {}"""'
                 .format(api_title, api_version))
    api_defs += "\n\n"
    schemas = apidata['components']['schemas']         # openapi 3.0
    for k in schemas.keys():
        api_defs += "\n"
        api_defs += print_definition(k, schemas[k], setdefaults=True)
        api_defs += "\n"

    print(api_defs)


