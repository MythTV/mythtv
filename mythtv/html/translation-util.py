#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# Python script to check, add, remove, amend strings in the translation JSON files
# used by the Angular MythTV webapp
#
# Example usage
#
# check and keep in sync the translation files
#   translation-util.py --check
#
# add new string
#   translation-util.py --add --key="common.test.new" --value="This is a new test string"
#
# remove a string from all translation files
#   translation-util.py --remove --key="common.test.new"
#
# amend an existing string
#   translation-util.py --amend --key="common.test.new" --value="This is an amended test string"
#
# list all the keys and strings for the German language file
#   translation-util.py --listkeys --language=de
#
#
# Required modules: googletrans and flatten_dict

import os, sys, re
import json
from optparse import OptionParser
from googletrans import Translator
from flatten_dict import flatten
from flatten_dict import unflatten

US_LANG = ('en_US', 'en_US.json', 'English US')

DEST_LANGS = [
            ('bg',    'bg.json',    'Bulgarian'),
            ('ca',    'ca.json',    'Catalan'),
            ('cs',    'cs.json',    'Czech'),
            ('da',    'da.json',    'Danish'),
            ('de',    'de.json',    'German'),
            ('el',    'el.json',    'Greek'),
            ('en_CA', 'en_CA.json', 'English Canadian'),
            ('en_GB', 'en_GB.json', 'English UK'),
            ('es_ES', 'es_ES.json', 'Spanish'),
            ('es',    'es.json',    'Spanish'),
            ('et',    'et.json',    'Estonian'),
            ('fi',    'fi.json',    'Finnish'),
            ('fr',    'fr.json',    'French'),
            ('he',    'he.json',    'Hebrew'),
            ('hr',    'hr.json',    'Croatian'),
            ('hu',    'hu.json',    'Hungarian'),
            ('is',    'is.json',    'Icelandic'),
            ('it',    'it.json',    'Italian'),
            ('ja',    'ja.json',    'Japanese'),
            ('no',    'nb.json',    'Norwegian'),
            ('nl',    'nl.json',    'Dutch'),
            ('pl',    'pl.json',    'Polish'),
            ('pt',    'pt.json',    'Portuguese'),
            ('pt',    'pt_BR.json', 'Portuguese (Brazil)'),
            ('ru',    'ru.json',    'Russian'),
            ('sl',    'sl.json',    'Slovenian'),
            ('sv',    'sv.json',    'Swedish'),
            ('tr',    'tr.json',    'Turkish'),
            ('zh-cn', 'zh_CN.json', 'Chinese (simplified)' ),
            ('zh-tw', 'zh_HK.json', 'Chinese (traditional)')
        ]

def doCheckTranslations(us_flat, code, filename, desc, keylist:list):
    # open dest language file
    print("\nOpening file for language %s from %s" % (desc, filename))
    dest_f = open(translation_dir + filename)
    dest_dict = json.load(dest_f)
    dest_f.close()

    dest_flat = flatten(dest_dict, reducer='dot')

    for key, value in us_flat.items():
        destString = dest_flat.get(key)
        if destString == None:
            action = "Adding missing string"
        else:
            action = "Updating string"
        if destString == None or destString == "" or key in keylist:
            if isinstance(value,list):
                destString = []
                for element in value:
                    trans = translate(element, code)
                    destString.append(trans)
            else:
                destString = translate(value, code)
            print(action + " '%s' -> '%s'" % (value, destString))
            dest_flat[key] = destString
        # elif destString == "":
        #     destString = translate(value, code)
        #     print("Updating empty string '%s' -> '%s'" % (value, destString))
        #     dest_flat[key] = destString

    # Check for strings to be deleted
    for key in list(dest_flat):
        if key not in us_flat:
            print("Deleting string '%s' : '%s'" % (key, dest_flat[key]))
            del dest_flat[key]

    dest_dict = unflatten(dest_flat, splitter='dot')

    # save to destination file
    with open(translation_dir + filename, mode="w", encoding="utf8") as outfile:
        json.dump(dest_dict, outfile, indent=4, ensure_ascii=False, sort_keys=True)

def checkTranslations(keylist:list):
    # make sure the US JSON file has its keys sorted
    sortUSTrans()

    # re-open sorted US source file
    us_code, us_filename, us_desc = US_LANG
    print("Re-opening sorted source US language %s from %s" % (us_desc, us_filename))
    us_f = open(translation_dir + us_filename)
    us_dict = json.load(us_f)
    us_f.close()

    us_flat = flatten(us_dict, reducer='dot')

    # Check specific keys to force translate on
    error = False
    for key in keylist:
        if key not in us_flat:
            print ("Error invalid key: " + key)
            error = True
    if error:
        sys.exit(2)

    # loop through all the language files
    for dest_code, dest_filename, dest_desc in DEST_LANGS:
        doCheckTranslations(us_flat, dest_code, dest_filename, dest_desc, keylist)

    print("\nAll languages checked OK")

def replaceTemplateString(srcStr, srcTemplate, destStr, destTemplate, index):
    index += 1

    found = 0
    startpos = 0
    endpos = 0

    while True:
        startpos = destStr.find("{{", endpos)
        if startpos == -1:
            print("ERROR: bad template string found - using US English string")
            return srcStr

        endpos = destStr.find("}}", startpos)
        if endpos == -1:
            print("ERROR: bad template string found - using US English string")
            return srcStr

        found += 1

        if found == index:
            break

    result = destStr[0:startpos] + srcTemplate + destStr[endpos:]

    return result

def translate(src_text, lang):
    # just pass through the UK and Canadian English strings
    if lang == 'en_US' or lang == 'en_UK' or lang == 'en_CA':
        return src_text

    # just use Spanish for now
    if lang == 'es_ES':
        lang = 'es'

    # check for a template string which we don't want to translate
    srcMatches = re.findall(r"({.*?})", src_text)

    translation = translator.translate(src_text, dest=lang, src='en')

    result = translation.text

    destMatches = re.findall(r"({.*?})", result)

    if len(srcMatches) > 0 and len(srcMatches) == len(destMatches):
        # this assumes the order of the template strings is the same
        for index, match in enumerate(destMatches):
            result = result.replace(match, srcMatches[index], 1)

    return result

def sortUSTrans():
    # open source file
    src_code, src_filename, src_desc = US_LANG
    print("Opening source language %s from %s" % (src_desc, src_filename))
    src_f = open(translation_dir + src_filename)
    src_dict = json.load(src_f)
    src_f.close()

    # save to destination file
    with open(translation_dir + src_filename, mode="w", encoding="utf8") as outfile:
        json.dump(src_dict, outfile, indent=4, ensure_ascii=False, sort_keys=True)

    print("%s translation sorted and saved OK" % src_desc)

def doAddString(code, filename, desc, key, value):
    # open language file
    print("\nOpening file for language %s from %s" % (desc, filename))
    src_f = open(translation_dir + filename)
    src_dict = json.load(src_f)
    src_f.close()

    d = flatten(src_dict, reducer='dot')

    d[key] = value

    src_dict = unflatten(d, splitter='dot')

    # save to destination file
    with open(translation_dir + filename, mode="w", encoding="utf8") as outfile:
        json.dump(src_dict, outfile, indent=4, ensure_ascii=False, sort_keys=True)

def addString(key, value):
    print("Adding key: %s and string: '%s' " % (key, value))

    # add to the US English file
    us_code, us_filename, us_desc = US_LANG
    doAddString(us_code, us_filename, us_desc, key, value)

    # loop through all the other language files
    for dest_code, dest_filename, dest_desc in DEST_LANGS:
        transvalue = translate(value, dest_code)
        doAddString(dest_code, dest_filename, dest_desc, key, transvalue)
        print("Adding string for key: %s - '%s' -> '%s'" % (key, value, transvalue))

    print("\nString added OK to all languages")

def doAmendString(code, filename, desc, key, value):
    # open language file
    print("\nOpening file for language %s from %s" % (desc, filename))
    src_f = open(translation_dir + filename)
    src_dict = json.load(src_f)
    src_f.close()

    d = flatten(src_dict, reducer='dot')

    oldValue = d.get(key)

    if oldValue == None:
        print("ERROR: key not found '%s'. Aborting!" % key)
        return

    print("Changing string from '%s' to '%s'" % (oldValue, value))

    d[key] = value

    src_dict = unflatten(d, splitter='dot')

    # save to destination file
    with open(translation_dir + filename, mode="w", encoding="utf8") as outfile:
        json.dump(src_dict, outfile, indent=4, ensure_ascii=False, sort_keys=True)

def amendString(key, value):
    print("Amending string for key: %s to value: %s" % (key, value))

    # update US English file
    us_code, us_filename, us_desc = US_LANG
    doAmendString(us_code, us_filename, us_desc, key, value)

    # loop through all the other language files
    for dest_code, dest_filename, dest_desc in DEST_LANGS:
        transvalue = translate(value, dest_code)
        doAmendString(dest_code, dest_filename, dest_desc, key, transvalue)

    print("\nString amended OK for all languages")

def doRemoveString(code, filename, desc, key):
    # open language file
    print("\nOpening file for language %s from %s" % (desc, filename))
    src_f = open(translation_dir + filename)
    src_dict = json.load(src_f)
    src_f.close()

    d = flatten(src_dict, reducer='dot')

    # check the key exists
    if d.get(key) == None:
        print("WARNING: key '%s' not found. Nothing to remove!")
        return

    if isinstance(d[key], str):
        print("Removing string: %s" % d[key])
        del d[key]
    elif isinstance(d[key], dict):
        # only delete the dict if it is empty
        if len(d[key]) == 0:
            print("Removing empty dict with key: %s" % key)
            del d[key]

    src_dict = unflatten(d, splitter='dot')

    # save to destination file
    with open(translation_dir + filename, mode="w", encoding="utf8") as outfile:
        json.dump(src_dict, outfile, indent=4, ensure_ascii=False, sort_keys=True)

def removeString(key):
    print("Removing key: %s from all translation files" % key)

    # remove key from US English file
    us_code, us_filename, us_desc = US_LANG
    doRemoveString(us_code, us_filename, us_desc, key)

    # loop through all the other language files
    for dest_code, dest_filename, dest_desc in DEST_LANGS:
        doRemoveString(dest_code, dest_filename, dest_desc, key)

    print("\nString removed OK from all languages")

def doFixTemplates(us_flat, code, filename, desc):
    # open language file
    print("\nChecking template strings for language %s from %s" % (desc, filename))
    src_f = open(translation_dir + filename)
    src_dict = json.load(src_f)
    src_f.close()

    d = flatten(src_dict)

    # loop through the US dictionary looking for templates
    for key, value in us_flat.items():
        # check for a template string which we don't want to translate

        # we are only interested in strings
        if not isinstance(value, str):
            continue

        srcMatches = re.findall(r"({{.*?}})", value)
        if len(srcMatches) > 0:
            # find the same template strings in the flattened source dictionary
            destString = d.get(key)
            if destString:
                destMatches = re.findall(r"({{.*?}})", destString)

                if len(srcMatches) == len(destMatches):
                    # this assumes the order of the template strings is the same
                    fixedString = destString

                    for index, match in enumerate(destMatches):
                        if match != srcMatches[index]:
                            fixedString = replaceTemplateString(value, srcMatches[index], fixedString, match, index)

                    if fixedString != destString:
                        print("Fixed string with template '%s' -> '%s'" % (destString, fixedString))
                        d[key] = fixedString
            else:
                transvalue = translate(value, code)
                print("Adding missing string '%s' -> %s" % (value, transvalue))
                d[key] = transvalue

    src_dict = unflatten(d)

    # save to destination file
    with open(translation_dir + filename, mode="w", encoding="utf8") as outfile:
        json.dump(src_dict, outfile, indent=4, ensure_ascii=False, sort_keys=True)

def fixTemplates():
    # load the US English source file
    us_code, us_filename, us_desc = US_LANG
    print("Opening source US language %s from %s" % (us_desc, us_filename))
    us_f = open(translation_dir + us_filename)
    us_dict = json.load(us_f)
    us_f.close()

    us_flat = flatten(us_dict)

    # loop through all the other language files
    for dest_code, dest_filename, dest_desc in DEST_LANGS:
        doFixTemplates(us_flat, dest_code, dest_filename, dest_desc)

def listKeys(lang):
    # find language
    found = False

    code, filename, desc = US_LANG

    if lang == code:
        found = True

    if not found:
        for code, filename, desc in DEST_LANGS:
            if lang == code:
                found = True
                break
    if not found:
        print("ERROR: language code not recognized '%s' - Aborting!" % lang)
        sys.exit(1)

    print("Showing keys and string for language %s from %s" % (desc, filename))

    src_f = open(translation_dir + filename)
    src_dict = json.load(src_f)
    src_f.close()

    d = flatten(src_dict, reducer='dot')

    for key, value in d.items():
        print("{:<50} {:<100}".format(key, str(value)))

def listLanguages():
    print("The listkeys command supports these language codes:-")
    for code,file,name in DEST_LANGS:
        print("    {0:6}  {1}".format(code,name))

if __name__ == '__main__':

    global translator
    translator = Translator()

    global translation_dir
    translation_dir = os.path.dirname(os.path.abspath(sys.argv[0])) + '/assets/i18n/'

    parser = OptionParser()

    parser.add_option('-t', "--check", action="store_true", default=False,
                      dest="check", help="Check all language files for missing or empty strings and use Google translate on them. Write out the file with strings sorted. Any strings that were manually deleted from the en_US file are deleted from the other language files.")
    parser.add_option('-F', "--flag", default="",metavar="KEYS",
                      dest="flag", help="Requires list of keys, space separated, in quotes as one parameter. Flag the listed keys as changed so they are re-translated with the --check option. This parameter also runs the --check process even if it was not requested.")
    parser.add_option('-s', "--sort", action="store_true", default=False,
                      dest="sort", help="Sort all the keys in the US English language file.")
    parser.add_option('-a', "--add", action="store_true", default=False,
                      dest="add", help="Add a new string to all the language files. Needs key and value. A key can also be added by manually adding it to the en_US file and running the --check option.")
    parser.add_option('-r', "--remove", action="store_true", default=False,
                      dest="remove", help="Remove the key from all translation files. Needs key. A key can also be removed from all translation files by manually removing it from the en_US file and running the --check option.")
    parser.add_option('-c', "--amend", action="store_true", default=False,
                      dest="amend", help="Amend the strings in all translation files for the given key. Needs key and value. Strings can also be updated in all files by manually updating the en_US file and running with the --flag option and a list of keys of updated strings.")
    parser.add_option('-l', "--listkeys", action="store_true", default=False,
                      dest="listkeys", help="List all keys and strings from a language file. [default: 'en_US'].")
    parser.add_option('-i', "--listlanguages", action="store_true", default=False,
                      dest="listlangs", help="List all supported languages")
    parser.add_option('-L', "--language", metavar="LANGUAGE", default=None,
                      dest="language", help="The language to show keys/values for.")
    parser.add_option('-k','--key', metavar="KEY", default=None,
                      dest="key", help=("The key used by add/remove/amend options."))
    parser.add_option('-v','--value', metavar="VALUE", default=None,
                      dest="value", help=("The string value used by add/amend option."))
    parser.add_option('-d','--datadir', metavar="DIR", default=None,
                      dest="datadir", help=("The location of the JSON language files. [default: '" + translation_dir + "']"))
    parser.add_option('-f','--fixtemplates', action="store_true", default=None,
                      dest="fixtemplates", help=("Check all language files for broken template strings."))

    # If no option is supplied go to help
    if len(sys.argv) == 1:
        sys.argv.append("-h")

    opts, args = parser.parse_args()

    if opts.datadir:
        translation_dir = opts.datadir
        print("Using translation directory '%s'" % translation_dir)

    if opts.listlangs:
        listLanguages()
        sys.exit(0)
    elif opts.check or opts.flag:
        keylist = opts.flag.split()
        checkTranslations(keylist)
        sys.exit(0)
    elif opts.sort:
        sortUSTrans()
        sys.exit(0)
    elif opts.add:
        if not opts.key:
            print("'add' option requires 'key' option")
            sys.exit(1)

        if not opts.value:
            print("'add' options requires 'value' option")
            sys.exit(1)

        key = opts.key
        value = opts.value

        addString(key, value)
        sys.exit(0)

    elif opts.amend:
        if not opts.key:
            print("'amend' options requires 'key' option")
            sys.exit(1)

        if not opts.value:
            print("'amend' options requires 'value' option")
            sys.exit(1)

        key = opts.key
        value = opts.value

        amendString(key, value)
        sys.exit(0)

    elif opts.remove:
        if not opts.key:
            print("'remove' option requires 'key' option")
            sys.exit(1)

        key = opts.key

        removeString(key)
        sys.exit(0)

    elif opts.fixtemplates:
        fixTemplates()
        sys.exit(0)

    elif opts.listkeys:
        lang = "en_US"

        if opts.language:
            lang = opts.language

        listKeys(lang)
        sys.exit(0)

    sys.exit(0)
