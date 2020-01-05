#
# python_pathfix - adjust python shebang paths
#

from __future__ import print_function
from __future__ import unicode_literals
import sys
import os
from stat import ST_MODE
import getopt

def main():

    # Pick up default interpreter (ours)
    python_interpreter = os.path.normcase(sys.executable).encode()

    # Don't bother reporting
    verbose = False

    # Set default return code
    ret = 0

    usage = ('usage: %s -v -i /python_interpreter file-or-directory ...\n' % sys.argv[0])

    try:
        opts, args = getopt.getopt(sys.argv[1:], 'i:v')
    except getopt.GetoptError as msg:
        sys.stderr.write(str(msg) + '\n')
        sys.stderr.write(usage)
        sys.exit(1)
    for o, a in opts:
        if o == '-i':
            python_interpreter = a.encode()
        if o == '-v':
            verbose = True

    if not python_interpreter or not python_interpreter.startswith(b'/'):
        sys.stderr.write('-i interpreter option invalid (must be specified as a full path)\n')
        sys.stderr.write(usage)
        sys.exit(1)

    if not args:
        sys.stderr.write('file or directories not specified\n')
        sys.stderr.write(usage)
        sys.exit(1)

    for arg in args:
        if os.path.islink(arg):
            continue
        if os.path.isfile(arg):
            if pathfix(filename=arg,
                       verbose=verbose,
                       python_interpreter=python_interpreter):
                ret = 1
        if os.path.isdir(arg):
            for root, subdirs, files in os.walk(arg):
                for f in files:
                    filename = os.path.join(root, f)
                    if os.path.islink(filename):
                        continue
                    if os.path.isfile(filename):
                        if pathfix(filename=filename,
                                   verbose=verbose,
                                   python_interpreter=python_interpreter):
                            ret = 1

    sys.exit(ret)

def pathfix(filename=None, verbose=False, python_interpreter='/usr/bin/python'):
    if not filename:
        if verbose:
            sys.stdout.write('filename not provided\n')
        return 1
    # open input file
    try:
        infile = open(filename, 'rb')
    except IOError as msg:
        sys.stderr.write('%s: open failed: %r\n' % (filename, msg))
        return 1
    # process first line
    try:
        firstline = infile.readline()
    except IOError as msg:
        sys.stderr.write('%s: read failed: %r\n' % (filename, msg))
        try:
            infile.close()
        except IOError as msg:
            sys.stderr.write('%s: close failed: %r\n' % (filename, msg))
        return 1
    if firstline.rstrip(b'\n').find(b' -') != -1:
        existingargs = firstline.rstrip(b'\n')[firstline.rstrip(b'\n').find(b' -'):]
    else:
        existingargs = b''
    newline = b'#!' + python_interpreter + existingargs + b'\n'
    if (not firstline.startswith(b'#!')) or (b"python" not in firstline) or (firstline == newline):
        if verbose:
            sys.stdout.write('%s: unchanged\n' % (filename))
        try:
            infile.close()
        except IOError as msg:
            sys.stderr.write('%s: close failed: %r\n' % (filename, msg))
        return 0
    # create temporary output
    head, tail = os.path.split(filename)
    tempname = os.path.join(head, '@' + tail)
    try:
        outfile = open(tempname, 'wb')
    except IOError as msg:
        sys.stderr.write('%s: create failed: %r\n' % (tempname, msg))
        try:
            infile.close()
        except IOError as msg:
            sys.stderr.write('%s: close failed: %r\n' % (filename, msg))
        return 1
    if verbose:
        sys.stdout.write('%s: updating\n' % (filename))
    # write first new line to temporary output
    try:
        outfile.write(newline)
    except IOError as msg:
        sys.stderr.write('%s: write failed: %r\n' % (tempname, msg))
        try:
            infile.close()
        except IOError as msg:
            sys.stderr.write('%s: close failed: %r\n' % (filename, msg))
        try:
            outfile.close()
        except IOError as msg:
            sys.stderr.write('%s: close failed: %r\n' % (tempname, msg))
        return 1
    # copy rest of file
    while True:
        try:
            chunk = infile.read(32678)
        except IOError as msg:
            sys.stderr.write('%s: read failed: %r\n' % (filename, msg))
            try:
                infile.close()
            except IOError as msg:
                sys.stderr.write('%s: close failed: %r\n' % (filename, msg))
            try:
                outfile.close()
            except IOError as msg:
                sys.stderr.write('%s: close failed: %r\n' % (tempname, msg))
            try:
                os.remove(tempname)
            except OSError as msg:
                sys.stderr.write('%s: remove failed: %r\n' % (tempname, msg))
            return 1
        if not chunk:
            break
        try:
            outfile.write(chunk)
        except IOError as msg:
            sys.stderr.write('%s: write failed: %r\n' % (tempname, msg))
            try:
                infile.close()
            except IOError as msg:
                sys.stderr.write('%s: close failed: %r\n' % (filename, msg))
            try:
                outfile.close()
            except IOError as msg:
                sys.stderr.write('%s: close failed: %r\n' % (tempname, msg))
            try:
                os.remove(tempname)
            except OSError as msg:
                sys.stderr.write('%s: remove failed: %r\n' % (tempname, msg))
            return 1
    # close files
    try:
        infile.close()
    except IOError as msg:
        sys.stderr.write('%s: close failed: %r\n' % (filename, msg))
        try:
            outfile.close()
        except IOError as msg:
            sys.stderr.write('%s: close failed: %r\n' % (tempname, msg))
        try:
            os.remove(tempname)
        except OSError as msg:
            sys.stderr.write('%s: remove failed: %r\n' % (tempname, msg))
        return 1
    try:
        outfile.close()
    except IOError as msg:
        sys.stderr.write('%s: close failed: %r\n' % (tempname, msg))
        try:
            os.remove(tempname)
        except OSError as msg:
            sys.stderr.write('%s: remove failed: %r\n' % (tempname, msg))
        return 1
    # copy the file's mode to the temp file
    try:
        statbuf = os.stat(filename)
        os.chmod(tempname, statbuf[ST_MODE] & 0o7777)
    except OSError as msg:
        sys.stderr.write('%s: warning, chmod failed: %r\n' % (tempname, msg))
    # Remove original file
    try:
        os.remove(filename)
    except OSError as msg:
        sys.stderr.write('%s: remove failed: %r\n' % (filename, msg))
        try:
            os.remove(tempname)
        except OSError as msg:
            sys.stderr.write('%s: remove failed: %r\n' % (tempname, msg))
        return 1
    # move the temp file to the original file
    try:
        os.rename(tempname, filename)
    except OSError as msg:
        sys.stderr.write('%s: rename failed: %r\n' % (filename, msg))
        # leave the tempname alone (might be used for recovery)
        return 1
    return 0

if __name__ == '__main__':
    main()
