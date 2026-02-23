#!/usr/bin/env python
#
# Ported from mythlink.pl @commit a527db90b27cbc45a1fbec81f6dd2e7511e66bd4
#
# Creates symlinks to mythtv recordings using more-human-readable filenames.
# See --help for instructions.
#
# Automatically detects database settings from mysql.txt, and loads
# the mythtv recording directory from the database (code from nuvexport).
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @license   GPL
#

# Includes
#     use DBI
import argparse, os, re, sys
from functools import partial

import MythTV
from MythTV import FileOps
from MythTV.mythproto import check_ipv6, findfile

# 
# # Some variables we'll use here
#     our ($dest, $format, $usage, $underscores, $live, $rename, args.maxlength)
#     our ($chanid, $starttime, $filename)
#     our ($dformat, $dseparator, $dreplacement, $separator, $replacement)
#     our ($db_host, $db_user, $db_name, $db_pass, $video_dir, $verbose)
#     our ($hostname, $dbh, $sh, $q, $count, $base_dir)
# 
# 
# # Provide default values for GetOptions
#     $format      = $dformat
#     $separator   = $dseparator
#     $replacement = $dreplacement
#     args.maxlength   = -1

class MythBEExt( MythTV.MythBE ):
    @FileOps._ProgramQuery('QUERY_RECORDINGS Descending', sorted=True)
    def getRecordingsDesc(self, pg):
        """
        Returns a list of all Program objects which have already recorded
        """
        return pg
    
    
# Load the cli options
parser = argparse.ArgumentParser()
parser.add_argument('--link', '--destination', '--path', action='store', help='''Specify the directory for the links.  If no pathname is given, links will
    be created in the show_names directory inside of the current mythtv data
    directory on this machine.  eg:

    /var/video/show_names/

    WARNING: ALL symlinks within the destination directory and its
    subdirectories (recursive) will be removed.''', dest='dest')
parser.add_argument('--chanid', action='store', help='''Create a link only for the specified recording file. Use with --starttime
    to specify a recording. This argument may be used with the event-driven
    notification system's "Recording started" event or in a post-recording
    user job.''')
parser.add_argument('--starttime', action='store', help='''Create a link only for the specified recording file. Use with --chanid
    to specify a recording. This argument may be used with the event-driven
    notification system's "Recording started" event or in a post-recording
    user job.''')
parser.add_argument('--filename', action='store', help='''Create a link only for the specified recording file. This argument may be
    used with the event-driven notification system's "Recording started" event
    or in a post-recording user job.''')
parser.add_argument('--format', action='store', default='%T %- %Y-%m-%d, %g-%i %A %- %S', help='''%%T   = Title (show name)
    %%S   = Subtitle (episode name)
    %%R   = Description
    %%eS  = Season (leading zero)
    %%eE  = Episode (leading zero)
    %%eP  = Part Number (Suppressed if not present render as per --renderpart)
    %%in  = Internet reference number
    %%C   = Category
    %%Ct  = Category Type
    %%U   = RecGroup
    %%hn  = Hostname of the machine where the file resides
    %%c   = Channel:  MythTV chanid
    %%cn  = Channel:  channum
    %%cc  = Channel:  callsign
    %%cN  = Channel:  channel name
    %%y   = Recording start time:  year, 2 digits
    %%Y   = Recording start time:  year, 4 digits
    %%n   = Recording start time:  month
    %%m   = Recording start time:  month, leading zero
    %%j   = Recording start time:  day of month
    %%d   = Recording start time:  day of month, leading zero
    %%g   = Recording start time:  12-hour hour
    %%G   = Recording start time:  24-hour hour
    %%h   = Recording start time:  12-hour hour, with leading zero
    %%H   = Recording start time:  24-hour hour, with leading zero
    %%i   = Recording start time:  minutes
    %%s   = Recording start time:  seconds
    %%a   = Recording start time:  am/pm
    %%A   = Recording start time:  AM/PM
    %%ey  = Recording end time:  year, 2 digits
    %%eY  = Recording end time:  year, 4 digits
    %%en  = Recording end time:  month
    %%em  = Recording end time:  month, leading zero
    %%ej  = Recording end time:  day of month
    %%ed  = Recording end time:  day of month, leading zero
    %%eg  = Recording end time:  12-hour hour
    %%eG  = Recording end time:  24-hour hour
    %%eh  = Recording end time:  12-hour hour, with leading zero
    %%eH  = Recording end time:  24-hour hour, with leading zero
    %%ei  = Recording end time:  minutes
    %%es  = Recording end time:  seconds
    %%ea  = Recording end time:  am/pm
    %%eA  = Recording end time:  AM/PM
    %%py  = Program start time:  year, 2 digits
    %%pY  = Program start time:  year, 4 digits
    %%pn  = Program start time:  month
    %%pm  = Program start time:  month, leading zero
    %%pj  = Program start time:  day of month
    %%pd  = Program start time:  day of month, leading zero
    %%pg  = Program start time:  12-hour hour
    %%pG  = Program start time:  24-hour hour
    %%ph  = Program start time:  12-hour hour, with leading zero
    %%pH  = Program start time:  24-hour hour, with leading zero
    %%pi  = Program start time:  minutes
    %%ps  = Program start time:  seconds
    %%pa  = Program start time:  am/pm
    %%pA  = Program start time:  AM/PM
    %%pey = Program end time:  year, 2 digits
    %%peY = Program end time:  year, 4 digits
    %%pen = Program end time:  month
    %%pem = Program end time:  month, leading zero
    %%pej = Program end time:  day of month
    %%ped = Program end time:  day of month, leading zero
    %%peg = Program end time:  12-hour hour
    %%peG = Program end time:  24-hour hour
    %%peh = Program end time:  12-hour hour, with leading zero
    %%peH = Program end time:  24-hour hour, with leading zero
    %%pei = Program end time:  minutes
    %%pes = Program end time:  seconds
    %%pea = Program end time:  am/pm
    %%peA = Program end time:  AM/PM
    %%oy  = Original Airdate:  year, 2 digits
    %%oY  = Original Airdate:  year, 4 digits
    %%on  = Original Airdate:  month
    %%om  = Original Airdate:  month, leading zero
    %%oj  = Original Airdate:  day of month
    %%od  = Original Airdate:  day of month, leading zero
    %%%%  = a literal %% character

    * The program start time is the time from the listings data and is not
      affected by when the recording started.  Therefore, using program start
      (or end) times may result in duplicate names.  In that case, the script
      will append a "counter" value to the name.

    * A suffix of .mpg or .nuv will be added where appropriate.

    * To separate links into subdirectories, include the / format specifier
      between the appropriate fields.  For example, "%%T/%%S" would create
      a directory for each title containing links for each recording named
      by subtitle.  You may use any number of subdirectories in your format
      specifier.''')
parser.add_argument('--live', action='store_true', help='Include live tv recordings.')
parser.add_argument('--separator', action='store', default='-', help='''The string used to separate sections of the link name.  Specifying the
    separator allows trailing separators to be removed from the link name and
    multiple separators caused by missing data to be consolidated. Indicate the
    separator character in the format string using either a literal character
    or the %%- specifier.''')
parser.add_argument('--replacement', action='store', default='-', help='''Characters in the link name which are not legal on some filesystems will
    be replaced with the given character

    illegal characters:  \\ : * ? < > | "''')
parser.add_argument('--rename', action='store_true', help='''Rename the recording files back to their default names.  If you had
    previously used mythrename.py to rename files (rather than creating links
    to the files), use this option to restore the file names to their default
    format.

    Renaming the recording files is no longer supported.  Instead, use
    http://www.mythtv.org/wiki/mythfs.py to create a FUSE file system that
    represents recordings using human-readable file names or use mythlink.pl to
    create links with human-readable names to the recording files.''')
parser.add_argument('--maxlength', action='store', type=int, help='''Ensure the link name is length or fewer characters.  If the link name is
    longer than length, truncate the name. Zero or any negative value for
    length disables length checking.

    Note that this option does not take into account the path length, so on a
    file system used by applications with small path limitations (such as
    Windows Explorer and Windows Command Prompt), you should specify a length
    that takes into account characters used by the path to the dest directory.''', default=0)

parser.add_argument('--usage', action='store_true', help='Show this help text.')
parser.add_argument('--underscores', action='store_true', help='Replace whitespace in filenames with underscore characters.')
parser.add_argument('--verbose', action='store_true', help='Print debug info.')
parser.add_argument('--renderpart', action='store', help='''Use the following as template for rendering part numbers. Use %%x as the place where the part number should go e.g. "(Part %%x)" or "pt%%x".''', default='pt%x')
parser.add_argument('--disablereplacement', action='store_true', help='Stops the disabling of the replacement texts')

args = parser.parse_args()
if args.usage:
    parser.print_usage()
    sys.exit()

# Ensure --chanid and --starttime were specified together, if at all
if (args.chanid or args.starttime) and not (args.chanid and args.starttime):
    raise Exception("The arguments --chanid and --starttime must be used together.")

# Ensure --maxlength specifies a reasonable value (though filenames may
# still be useless at such short lengths)
if args.maxlength > 0 and args.maxlength < 19:
    raise Exception("The --maxlength must be 20 or higher.")

# Check the separator and replacement characters for illegal characters
if args.separator is not None and re.match(r'(?:[\/\\:*?<>|"])', args.separator) is not None:
    raise Exception("The separator cannot contain any of the following characters:  /\\:*?<>|\"")
elif args.replacement is not None and re.match(r'(?:[\/\\:*?<>|"])', args.replacement) is not None:
    raise Exception("The replacement cannot contain any of the following characters:  /\\:*?<>|\"")

safe_sep = None
if args.separator is not None:
    safe_sep = re.sub(r'([^\w\s])', r'\1', args.separator)
safe_rep = None
if args.replacement is not None:
    safe_rep = re.sub(r'([^\w\s])', r'\1', args.replacement)
if args.disablereplacement:
    safe_rep = None

# Connect to mythbackend
#Myth = MythTV.MythTV()

# Connect to the database
dbh = MythTV.MythDB()
mbe = MythBEExt( db = dbh )

# Get the hostname of this machine
hostname = dbh.gethostname()

def GetLocalFilename(show):
    file = show.filename if show.filename.startswith('myth://') else \
                    (show.hostname, show.storagegroup, show.filename)
    mode = 'r'
    db   = MythTV.DBCache(dbh)

    reuri = re.compile(\
        'myth://((?P<group>.*)@)?(?P<host>[\[\]a-zA-Z0-9_\-\.]*)(:[0-9]*)?/(?P<file>.*)')
    reip = re.compile('(?:\d{1,3}\.){3}\d{1,3}')

    if mode not in ('r','w'):
        raise TypeError("File I/O must be of type 'r' or 'w'")

    # process URI (myth://<group>@<host>[:<port>]/<path/to/file>)
    match = None
    try:
        match = reuri.match(file)
    except:
        pass

    if match:
        host = match.group('host')
        filename = match.group('file')
        sgroup = match.group('group')
        if sgroup is None:
            sgroup = 'Default'
    elif len(file) == 3:
        host, sgroup, filename = file
    else:
        raise MythTV.MythError('Invalid FileTransfer input string: '+file)

    # get full system name
    host = host.strip('[]')
    if reip.match(host) or check_ipv6(host):
        host = db._gethostfromaddr(host)

    # search for file in local directories
    sg = findfile(filename, sgroup, db)
    if sg is None:
        return None

    return sg.dirname+filename

def FindRecordingDir(basename):
    for sgroup in dbh.getStorageGroup():
        if os.path.exists(os.path.join(sgroup.dirname, basename)):
            return sgroup.dirname

    return ''

# Check the length of the link name
def cut_down_name(name, suffix):
    if args.maxlength > 0:
        charsavailable = args.maxlength - len(suffix)
        if args.charsavailable > 0:
            name = name[0:charsavailable]
        else:
            name = ''
    return name

# Print the message, but only if verbosity is enabled
def vprint(message):
    if not args.verbose:
        return
    print(message)
    
    
def padNr(padBy, toPad):
    if toPad is None:
        return ''
    try:
        return str(format(int(toPad), padBy))
    except:
        return ''

def partNumber(partNumber):
    if partNumber is None or args.renderpart is None or partNumber == 0:
        return ''
    return str(args.renderpart).replace('%x', str(partNumber))
    
def showFormatPathExt(show, path, replace=None):
    for (tag, data, fn) in (('eS','season',partial(padNr, '02')), ('eE','episode', partial(padNr, '02')),
                            ('eP','part_number', partNumber)):
            path = path.replace('%'+tag, fn(show[data]))
    return show.formatPath(path, replace)

# Rename the file back to default format
def do_rename():
    for show in mbe.getRecordingsDesc():
        foundFile = GetLocalFilename(show)
        # File doesn't exist locally
        if foundFile is None or not os.path.isfile(foundFile):
            continue
        
        # Format the name
        name = showFormatPathExt(show, '%c_%Y%m%d%H%i%s')
        
        # Get a shell-safe version of the filename (yes, I know it's not needed in this case, but I'm anal about such things)
        _safe_file = "'" + re.sub('\'', '\\\'', foundFile) + "'"

        # Figure out the suffix
        _filename, suffix = os.path.splitext(foundFile)
        name = name[0:len(name)-len(suffix)]
        basename = os.path.basename(foundFile)
        
        # Rename the file, but only if it's a real file
        if suffix is None or len(suffix) == 0:
            continue
        
        # Check for duplicates
        video_dir = FindRecordingDir(basename)
        if os.path.isfile(os.path.join(video_dir, name + suffix)):
            count = 2
            while os.path.isfile(os.path.join(video_dir, name + "." + str(count) + suffix)):
                count = count + 1
            name += "." + str(count)
        name += suffix
        
        originalFilename = foundFile 
        show.filename = show.filename.replace(basename, name)
        show.update()
        
        try:
            os.rename(originalFilename, os.path.join(video_dir, name))
        except:
            show.filename = originalFilename
            show.update()
            raise

        vprint(originalFilename + "\t-> " + show.filename)
        
        # Rename previews
        for thumb in os.listdir(video_dir):
            if thumb.endswith(".png"):
                match = re.match(r'^' + basename + r'((?:\.\d+)?(?:\.\d+x\d+(?:x\d+)?)?)\.png$', thumb)
                if not match:
                    continue
                dim = match.group(1)
                try:
                    os.rename(os.path.join(video_dir, thumb), os.path.join(video_dir, name + dim + ".png"))
                except:
                    # If the rename fails, try to delete the preview from the
                    # cache (it will automatically be re-created with the
                    # proper name, when necessary)
                    try:
                        os.remove(os.path.join(video_dir, thumb))
                    except:
                        vprint("Unable to rename preview image: '" + os.path.join(video_dir, thumb) + "'.")
                     
# Only if we're renaming files back to "default" names
if args.rename:
    do_rename()
    sys.exit()

# Get our base location
base_dir = FindRecordingDir('show_names')

if base_dir == '':
    base_dir = dbh.getStorageGroup().next().dirname

# Link destination
# Double-check the destination
dest = args.dest

# Alert the user
if dest is None:
    dest = os.path.join(base_dir, 'show_names')

vprint("Link destination directory: " + dest)

# Create nonexistent paths
if dest is not None and not os.path.exists(dest):
    os.makedirs(dest)

# Bad path
if dest is None or not os.path.isdir(dest):
    raise Exception(str(dest) + " is not a directory.")

# Delete old links/directories unless linking only one recording
if args.filename and args.chanid:
    for root, dirnames, filenames in os.walk(dest):
        for filename in filenames:
            if os.path.islink(os.path.join(root, filename)):
                os.remove(os.path.join(root, filename))
            
    # Delete empty directories (should this be an option?)
    # Let this fail silently for non-empty directories
    for root, dirnames, filenames in os.walk(dest, topdown=False):
        for dirname in dirnames:
            os.rmdir(os.path.join(dirname, filename))

# Create symlinks for the files on this machine
rows = None
if args.chanid:
    rows = [ mbe.getRecording(args.chanid, args.starttime) ]
else:
    rows = mbe.getRecordingsDesc()
    
for show in rows:
    foundFile = GetLocalFilename(show)

    # File doesn't exist locally
    if foundFile is None or not os.path.exists(foundFile):
        continue
    
    # Skip LiveTV recordings?
    if not (args.live or show.recgroup != 'LiveTV'):
        continue

    # Check if this is the file to link if only linking one file
    if args.filename and not (os.basename(foundFile) == args.filename or
                     foundFile == args.filename):
        continue
    elif args.chanid:
        if show.chanid != args.chanid:
            continue
        recstartts = show.recstartts
        
        # Check starttime in MythTV time format (yyyy-MM-ddThh:mm:ss)
        if recstartts != args.starttime:
            # Check starttime in ISO time format (yyyy-MM-dd hh:mm:ss)
            recstartts = recstartts.replace('T', ' ')
            if recstartts != args.starttime:
                # Check starttime in job queue time format (yyyyMMddhhmmss)
                recstartts = re.sub(r'[\- :]', '', recstartts)
                if recstartts != args.starttime:
                    continue
                
    # Format the name
    name = showFormatPathExt(show, args.format,safe_rep)
    
    # Get a shell-safe version of the filename (yes, I know it's not needed in this case, but I'm anal about such things)
    _safe_file = "'" + re.sub('\'', '\\\'', foundFile) + "'"
    # Figure out the suffix
    _filename, suffix = os.path.splitext(foundFile)
    name = name[0:len(name)-len(suffix)]
    basename = os.path.basename(foundFile)
    
    # Check the link name's length
    name = cut_down_name(name, suffix)
    # Link destination
    # Check for duplicates
    if name and os.path.isfile(os.path.join(dest, name + suffix)):
        if ((args.filename and not args.chanid) or (not os.path.islink(os.path.join(dest, name + suffix)))):
            count = 2
            name = cut_down_name(name, "." + str(count) + suffix)
            while name and os.path.isfile(os.path.join(dest, name + "." + str(count) + suffix)): 
                count = count + 1
                name = cut_down_name(name, "." + str(count) + suffix)
            if name:
                name += "." + str(count)
        else:
            os.remove(os.path.join(dest, name + suffix))
        
    if not name:
        vprint("Unable to represent recording; maxlength too small.")
        continue
    
    name += suffix

    # Create the link
    directory = os.path.dirname(os.path.join(dest, name))
    if not os.path.isdir(directory):
        os.makedirs(directory)
        
    os.symlink(foundFile, os.path.join(dest, name))
    vprint(os.path.join(dest, name))
    
