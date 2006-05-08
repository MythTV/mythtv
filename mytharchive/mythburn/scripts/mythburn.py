# mythburn.py
# The ported MythBurn scripts which feature:

# Burning of recordings (including HDTV) and videos
# of ANY format to DVDR.  Menus are created using themes
# and are easily customised.

# See mydata.xml for format of input file

# spit2k1
# 11 January 2006
# 6 Feb 2006 - Added into CVS for the first time

# paulh
# 4 May 2006 - Added into mythtv svn

#For this script to work you need to have...
#Python2.3.5
#python2.3-mysqldb
#python2.3-imaging (PIL)
#dvdauthor - v0.6.11
#ffmpeg - 0.4.6
#dvd+rw-tools - v5.21.4.10.8
#cdrtools - v2.01

#Optional (only needed for tcrequant)
#transcode - v1.0.2

#******************************************************************************
#******************************************************************************
#******************************************************************************

# version of script - change after each update
VERSION="0.1.20060508-2"

#useFIFO enables the use of FIFO nodes on Linux - it saves time and disk space
#during multiplex operations but not supported on Windows platforms
useFIFO=True

#do we always want to encode the audio streams to ac3
encodetoac3 = False

#if this is set to true them all mpeg2 mythtv recordings will be run though mythtranscode
#which may clean up some audio problems on some files especially DVB files (experimental)
alwaysRunMythtranscode = False

##You can use this debug flag when testing out new themes
##pick some small recordings, run them through as normal
##set this variable to True and then re-run the scripts
##the temp. files will not be deleted and it will run through
##very much quicker!
debug_secondrunthrough = False

#*********************************************************************************
#Dont change the stuff below!!
#*********************************************************************************
import os, string, socket, sys, getopt, traceback
import xml.dom.minidom
import Image, ImageDraw, ImageFont
import MySQLdb
import time, datetime, tempfile

# media types (should match the enum in mytharchivewizard.h)
DVD_SL = 0
DVD_DL = 1
DVD_RW = 2
FILE   = 3

dvdPAL=(720,576)
dvdNTSC=(720,480)

dvdPALHalfD1="352x576"
dvdNTSCHalfD1="352x480"
dvdPALD1="%sx%s" % (dvdPAL[0],dvdPAL[1])
dvdNTSCD1="%sx%s" % (dvdNTSC[0],dvdNTSC[1])

#Single and dual layer recordable DVD free space in MBytes
dvdrsize=(4489,8978)   

frameratePAL=25
framerateNTSC=29.97

#Just blank globals at startup
temppath=""
logpath=""
scriptpath=""
sharepath=""
videopath=""
recordingpath=""
defaultsettings=""
videomode=""
gallerypath=""
musicpath=""
dateformat=""
timeformat=""
dbVersion=""
preferredlang1=""
preferredlang2=""

#name of the default job file
jobfile="mydata.xml"

#progress log filename and file object
progresslog = ""
progressfile = open("/dev/null", 'w')

#default location of DVD drive
dvddrivepath = "/dev/dvd"

#default option settings
docreateiso = False
doburn = True
erasedvdrw = False
mediatype = DVD_SL
savefilename = ''

configHostname = socket.gethostname()

# job xml file
jobDOM = None

# theme xml file
themeDOM = None
themeName = ''

#Maximum of 10 theme fonts
themeFonts = [0,0,0,0,0,0,0,0,0,0]

def write(text, progress=True):
    """Simple place to channel all text output through"""
    sys.stdout.write(text + "\n")
    sys.stdout.flush()

    if progress == True and progresslog != "":
        progressfile.write(time.strftime("%Y-%m-%d %H:%M:%S ") + text + "\n")
        progressfile.flush()

def fatalError(msg):
    """Display an error message and exit app"""
    write("*"*60)
    write("ERROR: " + msg)
    write("*"*60)
    write("")
    sys.exit(0)

def getTempPath():
    """This is the folder where all temporary files will be created."""
    return temppath

def getIntroPath():
    """This is the folder where all intro files are located."""
    return os.path.join(sharepath, "mytharchive", "intro")

def getMysqlDBParameters():
    global mysql_host
    global mysql_user
    global mysql_passwd
    global mysql_db
    global configHostname

    f = tempfile.NamedTemporaryFile();
    result = os.spawnlp(os.P_WAIT, 'mytharchivehelper','mytharchivehelper',
                        '-p', f.name)
    if result <> 0:
        write("Failed to run mytharchivehelper to get mysql database parameters! "
              "Exit code: %d" % result)
        if result == 254: 
            fatalError("Failed to init mythcontext.\n"
                       "Please check the troubleshooting section of the README for ways to fix this error")

    f.seek(0)
    mysql_host = f.readline()[:-1]
    mysql_user = f.readline()[:-1]
    mysql_passwd = f.readline()[:-1]
    mysql_db = f.readline()[:-1]
    configHostname = f.readline()[:-1]
    f.close()
    del f

def getDatabaseConnection():
    """Returns a mySQL connection to mythconverg database."""
    return MySQLdb.connect(host=mysql_host, user=mysql_user, passwd=mysql_passwd, db=mysql_db)

def doesFileExist(file):
    """Returns true/false if a given file or path exists."""
    return os.path.exists( file )

def quoteFilename(filename):
    filename = filename.replace('"', '\\"')
    return '"%s"' % filename

def getText(node):
    """Returns the text contents from a given XML element."""
    if node.childNodes.length>0:
        return node.childNodes[0].data
    else:
        return ""

def getThemeFile(theme,file):
    return os.path.join(sharepath, "mytharchive", "themes", theme, file)

def getFontPathName(fontname):
    return os.path.join(sharepath, fontname)

def getItemTempPath(itemnumber):
    return os.path.join(getTempPath(),"%s" % itemnumber)

def validateTheme(theme):
    #write( "Checking theme", theme
    file = getThemeFile(theme,"theme.xml")
    write("Looking for: " + file)
    return doesFileExist( getThemeFile(theme,"theme.xml") )

def isResolutionHDTV(videoresolution):
    return (videoresolution[0]==1920 and videoresolution[1]==1080) or (videoresolution[0]==1280 and videoresolution[1]==720)

def isResolutionOkayForDVD(videoresolution):
    if videomode=="ntsc":
        return videoresolution==(720,480) or videoresolution==(704,480) or videoresolution==(352,480) or videoresolution==(352,240)
    else:
        return videoresolution==(720,576) or videoresolution==(704,576) or videoresolution==(352,576) or videoresolution==(352,288)

def getImageSize(sourcefile):
    myimage=Image.open(sourcefile,"r")
    return myimage.size

def deleteAllFilesInFolder(folder):
    """Does what it says on the tin!."""
    for root, dirs, deletefiles in os.walk(folder, topdown=False):
        for name in deletefiles:
                os.remove(os.path.join(root, name))

def checkCancelFlag():
    """Checks to see if the user has cancelled this run"""
    if os.path.exists(os.path.join(logpath, "mythburncancel.lck")):
        os.remove(os.path.join(logpath, "mythburncancel.lck"))
        write('*'*60)
        write("Job has been cancelled at users request")
        write('*'*60)
        sys.exit(1)

def runCommand(command):
    checkCancelFlag()
    result=os.system(command)
    checkCancelFlag()
    return result

def encodeMenu(background, tempvideo, music, tempmovie, xmlfile, finaloutput):
    if videomode=="pal":
        framespersecond=frameratePAL
    else:
        framespersecond=framerateNTSC

    #Generate 15 seconds of menu
    totalframes=int(15 * framespersecond)

    command = path_png2yuv[0] + " -n %s -v0 -I p -f %s -j '%s' | %s -b 5000 -a 3 -v 1 -f 8 -o '%s'" \
               % (totalframes, framespersecond, background, path_mpeg2enc[0], tempvideo)
    result = runCommand(command)
    if result<>0:
        fatalError("Failed while running png2yuv - %s" % command)

    command = path_mplex[0] + " -f 8 -v 0 -o '%s' '%s' '%s'" % (tempmovie, tempvideo, music)
    result = runCommand(command)
    if result<>0:
        fatalError("Failed while running mplex - %s" % command)

    command = path_spumux[0] + " -m dvd -s 0 '%s' < '%s' > '%s'" % (xmlfile, tempmovie, finaloutput)
    result = runCommand(command)
    if result<>0:
        fatalError("Failed while running spumux - %s" % command)

    os.remove(tempvideo)
    os.remove(tempmovie)

def getThemeConfigurationXML(theme):
    """Loads the XML file from disk for a specific theme"""

    #Load XML input file from disk
    themeDOM = xml.dom.minidom.parse( getThemeFile(theme,"theme.xml") )
    #Error out if its the wrong XML
    if themeDOM.documentElement.tagName != "mythburntheme":
        fatalError("Theme xml file doesn't look right (%s)" % theme)
    return themeDOM

def getLengthOfVideo(index):
    """Returns the length of a video file (in seconds)"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(getItemTempPath(index), 'streaminfo.xml'))

    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("Stream info file doesn't look right (%s)" % os.path.join(getItemTempPath(index), 'streaminfo.xml'))
    file = infoDOM.getElementsByTagName("file")[0]
    if file.attributes["duration"].value != 'N/A':
        duration = int(file.attributes["duration"].value)
    else:
        duration = 0;

    ### Temp hack until we can get an accurate playing time ###
    duration -= 60 * 5
    if duration <= 0:
        duration = 60

    return duration


def getFormatedLengthOfVideo(index):
    duration = getLengthOfVideo(index)

    minutes = int(duration / 60)
    seconds = duration % 60
    hours = int(minutes / 60)
    minutes %= 60

    return '%02d:%02d:%02d' % (hours, minutes, seconds)

def createVideoChapters(itemnum, numofchapters, lengthofvideo, getthumbnails):      
    """Returns numofchapters chapter marks even spaced through a certain time period"""
    segment=int(lengthofvideo / numofchapters)

    write( "Video length is %s seconds. Each chapter will be %s seconds" % (lengthofvideo,segment))

    chapters=""
    thumbList=""
    starttime=0
    count=1
    while count<=numofchapters:
        chapters+=time.strftime("%H:%M:%S",time.gmtime(starttime))

        thumbList+="%s," % starttime

        if numofchapters>1:
            chapters+="," 
        starttime+=segment
        count+=1

    if getthumbnails==True:
        extractVideoFrames( os.path.join(getItemTempPath(itemnum),"stream.mv2"),
            os.path.join(getItemTempPath(itemnum),"chapter-%1.jpg"), thumbList)

    return chapters

def getDefaultParametersFromMythTVDB():
    """Reads settings from MythTV database"""

    write( "Obtaining MythTV settings from MySQL database for hostname " + configHostname)

    #TVFormat is not dependant upon the hostname.
    sqlstatement="""select value, data from settings where value in('TVFormat', 'DBSchemaVer') 
                    or (hostname='""" + configHostname + """' and value in(
                        'RecordFilePrefix',
                        'VideoStartupDir',
                        'GalleryDir',
                        'MusicLocation',
                        'MythArchiveTempDir',
                        'MythArchiveFfmpegCmd',
                        'MythArchiveMplexCmd',
                        'MythArchiveDvdauthorCmd',
                        'MythArchiveMkisofsCmd',
                        'MythArchiveTcrequantCmd',
                        'MythArchiveMpg123Cmd',
                        'MythArchiveProjectXCmd',
                        'MythArchiveDVDLocation',
                        'MythArchiveGrowisofsCmd',
                        'MythArchivePng2yuvCmd',
                        'MythArchiveSpumuxCmd',
                        'MythArchiveMpeg2encCmd',
                        'ISO639Language0',
                        'ISO639Language1'
                        )) order by value"""

    #write( sqlstatement)

    # connect
    db = getDatabaseConnection()
    # create a cursor
    cursor = db.cursor()
    # execute SQL statement
    cursor.execute(sqlstatement)
    # get the resultset as a tuple
    result = cursor.fetchall()

    db.close()
    del db
    del cursor

    cfg = {}
    for i in range(len(result)):
       cfg[result[i][0]] = result[i][1]

    #bail out if we can't find the temp dir setting
    if not "MythArchiveTempDir" in cfg:
        fatalError("Can't find the setting for the temp directory. \nHave you run setup in the frontend?")
    return cfg

def getOptions(options):
    global doburn
    global docreateiso
    global erasedvdrw
    global mediatype
    global savefilename

    if options.length == 0:
        fatalError("Trying to read the options from the job file but none found?")
    options = options[0]

    doburn = options.attributes["doburn"].value != '0'
    docreateiso = options.attributes["createiso"].value != '0'
    erasedvdrw = options.attributes["erasedvdrw"].value != '0'
    mediatype = int(options.attributes["mediatype"].value)
    savefilename = options.attributes["savefilename"].value

    write("Options - mediatype = %d, doburn = %d, createiso = %d, erasedvdrw = %d" \
           % (mediatype, doburn, docreateiso, erasedvdrw))
    write("          savefilename = '%s'" % savefilename)

def getTimeDateFormats():
    """Reads date and time settings from MythTV database and converts them into python date time formats"""

    global dateformat
    global timeformat

    #DateFormat = 	ddd MMM d
    #ShortDateFormat = M/d
    #TimeFormat = h:mm AP


    write( "Obtaining date and time settings from MySQL database for hostname "+ configHostname)

    #TVFormat is not dependant upon the hostname.
    sqlstatement = """select value,data from settings where (hostname='"""  + configHostname \
                        + """' and value in (
                        'DateFormat',
                        'ShortDateFormat',
                        'TimeFormat'
                        )) order by value"""

    # connect
    db = getDatabaseConnection()
    # create a cursor
    cursor = db.cursor()
    # execute SQL statement
    cursor.execute(sqlstatement)
    # get the resultset as a tuple
    result = cursor.fetchall()
    #We must have exactly 3 rows returned or else we have some MythTV settings missing
    if int(cursor.rowcount)!=3:
        fatalError("Failed to get time formats from the DB")
    db.close()
    del db
    del cursor

    #Copy the results into a dictionary for easier use
    mydict = {}
    for i in range(len(result)):
        mydict[result[i][0]] = result[i][1]

    del result

    #At present we ignore the date time formats from MythTV and default to these
    #basically we need to convert the MythTV formats into Python formats
    #spit2k1 - TO BE COMPLETED!!

    #Date and time formats used to show recording times see full list at
    #http://www.python.org/doc/current/lib/module-time.html
    dateformat="%a %d %b %Y"    #Mon 15 Dec 2005
    timeformat="%I:%M %p"       #8:15 pm


def expandItemText(infoDOM, text, itemnumber, pagenumber, keynumber,chapternumber, chapterlist ):
    """Replaces keywords in a string with variables from the XML and filesystem"""
    text=string.replace(text,"%page","%s" % pagenumber)

    #See if we can use the thumbnail/cover file for videos if there is one.
    if getText( infoDOM.getElementsByTagName("coverfile")[0]) =="":
        text=string.replace(text,"%thumbnail", os.path.join( getItemTempPath(itemnumber), "thumbnail.jpg"))
    else:
        text=string.replace(text,"%thumbnail", getText( infoDOM.getElementsByTagName("coverfile")[0]) )

    text=string.replace(text,"%itemnumber","%s" % itemnumber )
    text=string.replace(text,"%keynumber","%s" % keynumber )

    text=string.replace(text,"%title",getText( infoDOM.getElementsByTagName("title")[0]) )
    text=string.replace(text,"%subtitle",getText( infoDOM.getElementsByTagName("subtitle")[0]) )
    text=string.replace(text,"%description",getText( infoDOM.getElementsByTagName("description")[0]) )
    text=string.replace(text,"%type",getText( infoDOM.getElementsByTagName("type")[0]) )

    text=string.replace(text,"%recordingdate",getText( infoDOM.getElementsByTagName("recordingdate")[0]) )
    text=string.replace(text,"%recordingtime",getText( infoDOM.getElementsByTagName("recordingtime")[0]) )

    text=string.replace(text,"%duration", getFormatedLengthOfVideo(itemnumber))

    text=string.replace(text,"%myfolder",getThemeFile(themeName,""))

    if chapternumber>0:
        text=string.replace(text,"%chapternumber","%s" % chapternumber )
        text=string.replace(text,"%chaptertime","%s" % chapterlist[chapternumber - 1] )
        text=string.replace(text,"%chapterthumbnail", os.path.join( getItemTempPath(itemnumber), "chapter-%s.jpg" % chapternumber))

    return text

def intelliDraw(drawer,text,font,containerWidth):
    """Based on http://mail.python.org/pipermail/image-sig/2004-December/003064.html"""
    #Args:
    #  drawer: Instance of "ImageDraw.Draw()"
    #  text: string of long text to be wrapped
    #  font: instance of ImageFont (I use .truetype)
    #  containerWidth: number of pixels text lines have to fit into.

    #write("containerWidth: %s" % containerWidth)
    words = text.split()
    lines = [] # prepare a return argument
    lines.append(words) 
    finished = False
    line = 0
    while not finished:
        thistext = lines[line]
        newline = []
        innerFinished = False
        while not innerFinished:
            #write( 'thistext: '+str(thistext))
            #write("textWidth: %s" % drawer.textsize(' '.join(thistext),font)[0])

            if drawer.textsize(' '.join(thistext),font)[0] > containerWidth:
                # this is the heart of the algorithm: we pop words off the current
                # sentence until the width is ok, then in the next outer loop
                # we move on to the next sentence. 
                if str(thistext).find(' ') != -1:
                    newline.insert(0,thistext.pop(-1))
                else:
                    # FIXME should truncate the string here
                    innerFinished = True
            else:
                innerFinished = True
        if len(newline) > 0:
            lines.append(newline)
            line = line + 1
        else:
            finished = True
    tmp = []
    for i in lines:
        tmp.append( ' '.join(i) )
    lines = tmp
    (width,height) = drawer.textsize(lines[0],font)
    return (lines,width,height)

def paintText( draw, x,y,width, height,text, font, colour, alignment):
    """Takes a piece of text and draws it onto an image inside a bounding box."""
    #The text is wider than the width of the bounding box

    lines,tmp,h = intelliDraw(draw,text,font,width)
    j = 0

    for i in lines:
        if (j*h) < (height-h):
            write( "Wrapped text  = " + i, False)

            if alignment=="left":
                indent=0
            elif  alignment=="center" or alignment=="centre":
                indent=(width/2) - (draw.textsize(i,font)[0] /2)
            elif  alignment=="right":
                indent=width - draw.textsize(i,font)[0]
            else:
                indent=0
            draw.text( (x+indent,y+j*h),i , font=font, fill=colour)
        else:
            write( "Truncated text = " + i, False)
        #Move to next line
        j = j + 1

def checkBoundaryBox(boundarybox, node):
    # We work out how much space all of our graphics and text are taking up
    # in a bounding rectangle so that we can use this as an automatic highlight
    # on the DVD menu   
    if getText(node.attributes["static"])=="False":
        if int(node.attributes["x"].value) < boundarybox[0]:
            boundarybox = int(node.attributes["x"].value),boundarybox[1],boundarybox[2],boundarybox[3]

        if int(node.attributes["y"].value) < boundarybox[1]:
            boundarybox = boundarybox[0],int(node.attributes["y"].value),boundarybox[2],boundarybox[3]

        if (int(node.attributes["x"].value)+int(node.attributes["w"].value)) > boundarybox[2]:
            boundarybox = boundarybox[0],boundarybox[1],int(node.attributes["x"].value)+int(node.attributes["w"].value),boundarybox[3]

        if (int(node.attributes["y"].value)+int(node.attributes["h"].value)) > boundarybox[3]:
            boundarybox = boundarybox[0],boundarybox[1],boundarybox[2],int(node.attributes["y"].value)+int(node.attributes["h"].value)

    return boundarybox

def loadFonts(themeDOM):
    global themeFonts

    #Find all the fonts
    nodelistfonts=themeDOM.getElementsByTagName("font")

    fontnumber=0
    for nodefont in nodelistfonts:
        fontname = getText(nodefont)
        fontsize = int(nodefont.attributes["size"].value)
        themeFonts[fontnumber]=ImageFont.truetype(getFontPathName(fontname),fontsize )
        write( "Loading font %s, %s size %s" % (fontnumber,getFontPathName(fontname),fontsize) )
        fontnumber+=1

def getFileInformation(file, outputfile):
    impl = xml.dom.minidom.getDOMImplementation()
    infoDOM = impl.createDocument(None, "fileinfo", None)
    top_element = infoDOM.documentElement

    # if the jobfile has amended file details use them
    details = file.getElementsByTagName("details")
    if details.length > 0:
        node = infoDOM.createElement("type")
        node.appendChild(infoDOM.createTextNode(file.attributes["type"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("filename")
        node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
        top_element.appendChild(node)   

        node = infoDOM.createElement("title")
        node.appendChild(infoDOM.createTextNode(details[0].attributes["title"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("recordingdate")
        node.appendChild(infoDOM.createTextNode(details[0].attributes["startdate"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("recordingtime")
        node.appendChild(infoDOM.createTextNode(details[0].attributes["starttime"].value))
        top_element.appendChild(node)   

        node = infoDOM.createElement("subtitle")
        node.appendChild(infoDOM.createTextNode(details[0].attributes["subtitle"].value))
        top_element.appendChild(node)   

        node = infoDOM.createElement("description")
        node.appendChild(infoDOM.createTextNode(getText(details[0])))
        top_element.appendChild(node)   

        node = infoDOM.createElement("rating")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("coverfile")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)

        #FIXME: add cutlist to details?
        node = infoDOM.createElement("cutlist")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)

    #recorded table contains
    #progstart, stars, cutlist, category, description, subtitle, title, chanid
    #2005-12-20 00:00:00, 0.0, 
    elif file.attributes["type"].value=="recording":
        # we need two versions of this depending on what db version we are using
        sqlstatement  = """SELECT progstart, stars, cutlist, category, description, subtitle, 
                           title, starttime, chanid
                           FROM recorded WHERE basename = '%s'""" % file.attributes["filename"].value

        # connect
        db = getDatabaseConnection()
        # create a cursor
        cursor = db.cursor()
        # execute SQL statement
        cursor.execute(sqlstatement)
        # get the resultset as a tuple
        result = cursor.fetchall()
        # get the number of rows in the resultset
        numrows = int(cursor.rowcount)
        #We must have exactly 1 row returned for this recording
        if numrows!=1:
            fatalError("Failed to get recording details from the DB for %s" % file.attributes["filename"].value)

        # iterate through resultset
        for record in result:
            #write( record[0] , "-->", record[1], record[2], record[3])
            write( "          " + record[6])
            #Create an XML DOM to hold information about this video file

            node = infoDOM.createElement("type")
            node.appendChild(infoDOM.createTextNode(file.attributes["type"].value))
            top_element.appendChild(node)

            node = infoDOM.createElement("filename")
            node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
            top_element.appendChild(node)   

            node = infoDOM.createElement("title")
            node.appendChild(infoDOM.createTextNode(record[6]))
            top_element.appendChild(node)

            #date time is returned as 2005-12-19 00:15:00            
            recdate=time.strptime( "%s" % record[0],"%Y-%m-%d %H:%M:%S")

            node = infoDOM.createElement("recordingdate")
            node.appendChild(infoDOM.createTextNode( time.strftime(dateformat,recdate)  ))
            top_element.appendChild(node)

            node = infoDOM.createElement("recordingtime")
            node.appendChild(infoDOM.createTextNode( time.strftime(timeformat,recdate)))
            top_element.appendChild(node)   

            node = infoDOM.createElement("subtitle")
            node.appendChild(infoDOM.createTextNode(record[5]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("description")
            node.appendChild(infoDOM.createTextNode(record[4]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("rating")
            node.appendChild(infoDOM.createTextNode("%s" % record[1]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("coverfile")
            node.appendChild(infoDOM.createTextNode(""))
            #node.appendChild(infoDOM.createTextNode(record[8]))
            top_element.appendChild(node)

            node = infoDOM.createElement("chanid")
            node.appendChild(infoDOM.createTextNode("%s" % record[8]))
            top_element.appendChild(node)

            #date time is returned as 2005-12-19 00:15:00 
            recdate=time.strptime( "%s" % record[7],"%Y-%m-%d %H:%M:%S")

            node = infoDOM.createElement("starttime")
            node.appendChild(infoDOM.createTextNode( time.strftime("%Y-%m-%dT%H:%M:%S", recdate)))
            top_element.appendChild(node)

            starttime = record[7]
            chanid = record[8]

            # find the cutlist if available
            sqlstatement  = """SELECT mark, type FROM recordedmarkup 
                               WHERE chanid = '%s' AND starttime = '%s' 
                               AND type IN (0,1) ORDER BY mark""" % (chanid, starttime)
            cursor = db.cursor()
            # execute SQL statement
            cursor.execute(sqlstatement)
            if cursor.rowcount > 0:
                node = infoDOM.createElement("hascutlist")
                node.appendChild(infoDOM.createTextNode("yes"))
                top_element.appendChild(node)
            else:
                node = infoDOM.createElement("hascutlist")
                node.appendChild(infoDOM.createTextNode("no"))
                top_element.appendChild(node)

        db.close()
        del db
        del cursor

    elif file.attributes["type"].value=="video":
        sqlstatement="""select title, director, plot, rating, inetref, year, 
                        userrating, length, coverfile from videometadata 
                        where filename='%s'""" % os.path.join(videopath, file.attributes["filename"].value)

        # connect
        db = getDatabaseConnection()
        # create a cursor
        cursor = db.cursor()
        # execute SQL statement
        cursor.execute(sqlstatement)
        # get the resultset as a tuple
        result = cursor.fetchall()
        # get the number of rows in the resultset
        numrows = int(cursor.rowcount)

        #title,director,plot,rating,inetref,year,userrating,length,coverfile
        #We must have exactly 1 row returned for this recording
        if numrows<>1:
            #Theres no record in the database so use a dummy row so we dont die!
            #title,director,plot,rating,inetref,year,userrating,length,coverfile
            record = file.attributes["filename"].value, "","",0,"","",0,0,""

        for record in result:
            write( "          " + record[0])

            node = infoDOM.createElement("type")
            node.appendChild(infoDOM.createTextNode(file.attributes["type"].value))
            top_element.appendChild(node)

            node = infoDOM.createElement("filename")
            node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
            top_element.appendChild(node)   

            node = infoDOM.createElement("title")
            node.appendChild(infoDOM.createTextNode(record[0]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("recordingdate")
            node.appendChild(infoDOM.createTextNode("%s" % record[5]))
            top_element.appendChild(node)

            node = infoDOM.createElement("recordingtime")
            #node.appendChild(infoDOM.createTextNode(""))
            top_element.appendChild(node)   

            node = infoDOM.createElement("subtitle")
            #node.appendChild(infoDOM.createTextNode(""))
            top_element.appendChild(node)   

            node = infoDOM.createElement("description")
            node.appendChild(infoDOM.createTextNode(record[2]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("rating")
            node.appendChild(infoDOM.createTextNode("%s" % record[6]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("cutlist")
            #node.appendChild(infoDOM.createTextNode(record[2]))
            top_element.appendChild(node)   

            node = infoDOM.createElement("coverfile")
            if record[8]!="" and record[8]!="No Cover":
                node.appendChild(infoDOM.createTextNode(record[8]))
            top_element.appendChild(node)   

        db.close()
        del db
        del cursor

    elif file.attributes["type"].value=="file":

        node = infoDOM.createElement("type")
        node.appendChild(infoDOM.createTextNode(file.attributes["type"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("filename")
        node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("title")
        node.appendChild(infoDOM.createTextNode(file.attributes["filename"].value))
        top_element.appendChild(node)

        node = infoDOM.createElement("recordingdate")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)

        node = infoDOM.createElement("recordingtime")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("subtitle")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("description")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("rating")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("cutlist")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

        node = infoDOM.createElement("coverfile")
        node.appendChild(infoDOM.createTextNode(""))
        top_element.appendChild(node)   

    WriteXMLToFile (infoDOM, outputfile)

def WriteXMLToFile(myDOM, filename):
    #Save the XML file to disk for use later on
    f=open(filename, 'w')
    f.write(myDOM.toxml())
    f.close()


def preProcessFile(file, folder):
    """Pre-process a single video/recording file."""

    write( "Pre-processing file '" + file.attributes["filename"].value + \
           "' of type '"+ file.attributes["type"].value+"'")

    #As part of this routine we need to pre-process the video:
    #1. check the file actually exists
    #2. extract information from mythtv for this file in xml file
    #3. Extract a single frame from the video to use as a thumbnail and resolution check
    mediafile=""

    if file.attributes["type"].value == "recording":
        mediafile = os.path.join(recordingpath, file.attributes["filename"].value)
    elif file.attributes["type"].value == "video":
        mediafile = os.path.join(videopath, file.attributes["filename"].value)
    elif file.attributes["type"].value == "file":
        mediafile = file.attributes["filename"].value
    else:
        fatalError("Unknown type of video file it must be 'recording', 'video' or 'file'.")

    if doesFileExist(mediafile) == False:
        fatalError("Source file does not exist: " + mediafile)

    #write( "Original file is",os.path.getsize(mediafile),"bytes in size")
    getFileInformation(file, os.path.join(folder, "info.xml"))

    getStreamInformation(mediafile, os.path.join(folder, "streaminfo.xml"))

    videosize = getVideoSize(os.path.join(folder, "streaminfo.xml"))

    write( "Video resolution is %s by %s" % (videosize[0], videosize[1]))

def encodeAudio(format, sourcefile, destinationfile, deletesourceafterencode):
    write( "Encoding audio to "+format)
    if format=="ac3":
        cmd=path_ffmpeg[0] + " -v 0 -y -i '%s' -f ac3 -ab 192 -ar 48000 '%s'" % (sourcefile, destinationfile)
        result=runCommand(cmd)

        if result!=0:
            fatalError("Failed while running ffmpeg to re-encode the audio to ac3\n"
                       "Command was %s" % cmd)
    else:
        fatalError("Unknown encodeAudio format " + format)

    if deletesourceafterencode==True:
        os.remove(sourcefile)

def multiplexMPEGStream(video, audio1, audio2, destination):
    """multiplex one video and one or two audio streams together"""

    write("Multiplexing MPEG stream to %s" % destination)

    if doesFileExist(destination)==True:
        os.remove(destination)

    if useFIFO==True:
        os.mkfifo(destination)
        mode=os.P_NOWAIT
    else:
        mode=os.P_WAIT

    checkCancelFlag()

    if not doesFileExist(audio2):
        write("Available streams - video and one audio stream")
        result=os.spawnlp(mode, path_mplex[0], path_mplex[1],
                    '-f', '8',
                    '-v', '0',
                    '-o', destination,
                    video,
                    audio1)
    else:
        write("Available streams - video and two audio streams")
        result=os.spawnlp(mode, path_mplex[0], path_mplex[1],
                    '-f', '8',
                    '-v', '0',
                    '-o', destination,
                    video,
                    audio1,
                    audio2)

    if useFIFO==True:
        write( "Multiplex started PID=%s" % result)
        return result
    else:
        if result != 0:
            fatalError("mplex failed with result %d" % result)

def getStreamInformation(filename, xmlFilename):
    """create a stream.xml file for filename"""
    filename = quoteFilename(filename)
    command = "mytharchivehelper -i %s %s" % (filename, xmlFilename)

    result = runCommand(command)

    if result <> 0:
        fatalError("Failed while running mytharchivehelper to get stream information from %s" % filename)

def getVideoSize(xmlFilename):
    """Get video width and height from stream.xml file"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(xmlFilename)
    #error out if its the wrong XML
    
    if infoDOM.documentElement.tagName != "file":
        fatalError("This info file doesn't look right (%s)." % xmlFilename)
    nodes = infoDOM.getElementsByTagName("video")
    if nodes.length == 0:
        fatalError("Didn't find any video elements in stream info file. (%s)" % xmlFilename)

    if nodes.length > 1:
        write("Found more than one video element in stream info file.!!!")
    node = nodes[0]
    width = int(node.attributes["width"].value)
    height = int(node.attributes["height"].value)

    return (width, height)

def runMythtranscode(chanid, starttime, destination, usecutlist):
    """Use mythtrancode to cut commercials and/or clean up an mpeg2 file"""

    if usecutlist == True:
        command = "mythtranscode --mpeg2 --honorcutlist -c %s -s %s -o %s" % (chanid, starttime, destination)
    else:
        command = "mythtranscode --mpeg2 -c %s -s %s -o %s" % (chanid, starttime, destination)

    result = runCommand(command)

    if (result != 0):
        write("Failed while running mythtranscode to cut commercials and/or clean up an mpeg2 file.\n"
              "Result: %d, Command was %s" % (result, command))
        return False;

    return True

def extractVideoFrame(source, destination, seconds):
    write("Extracting thumbnail image from %s at position %s" % (source, seconds))
    write("Destination file %s" % destination)

    if doesFileExist(destination) == False:

        if videomode=="pal":
            fr=frameratePAL
        else:
            fr=framerateNTSC

        source = quoteFilename(source)

        command = "mytharchivehelper -t  %s '%s' %s" % (source, seconds, destination)
        result = runCommand(command)
        if result <> 0:
            fatalError("Failed while running mytharchivehelper to get thumbnails.\n"
                       "Result: %d, Command was %s" % (result, command))
    try:
        myimage=Image.open(destination,"r")

        if myimage.format <> "JPEG":
            write( "Something went wrong with thumbnail capture - " + myimage.format)
            return (0L,0L)
        else:
            return myimage.size
    except IOError:
        return (0L, 0L)

def extractVideoFrames(source, destination, thumbList):
    write("Extracting thumbnail images from: %s - at %s" % (source, thumbList))
    write("Destination file %s" % destination)

    source = quoteFilename(source)

    command = "mytharchivehelper -v important -t %s '%s' %s" % (source, thumbList, destination)
    result = runCommand(command)
    if result <> 0:
        fatalError("Failed while running mytharchivehelper to get thumbnails")

def encodeVideoToMPEG2(source, destvideofile, video, audio1, audio2, aspectratio):
    """Encodes an unknown video source file eg. AVI to MPEG2 video and AC3 audio, use ffmpeg"""
    #MPEG-2 resolutions: 720x480, 720x576, 352x480, 352x576

    if videomode=="ntsc":
        encoderesolution=dvdNTSCD1
    else:
        encoderesolution=dvdPALD1

    #192 is the kbit rate for the ac3 audio
    #3000 is the kbit rate for the MPEG2 video

    source = quoteFilename(source)

    command = path_ffmpeg[0] + " -v 1 -i %s -r %s -target dvd -b 3000 -s %s -ab 192 -copyts "   \
              "-aspect %s %s"  % (source, videomode, encoderesolution, aspectratio, destvideofile)

    #add second audio track if required
    if audio2[AUDIO_ID] != -1:
        command += " -newaudio" 

    #make sure we get the correct stream(s) that we want
    command += " -map 0:%d -map 0:%d " % (video[VIDEO_INDEX], audio1[AUDIO_INDEX])
    if audio2[AUDIO_ID] != -1:
        command += "-map 0:%d" % (audio2[AUDIO_INDEX])

    write(command)

    result = runCommand(command)

    if result!=0:
        fatalError("Failed while running ffmpeg to re-encode video.\n"
                   "Command was %s" % command)

def runDVDAuthor():
    write( "Starting dvdauthor")
    checkCancelFlag()
    result=os.spawnlp(os.P_WAIT, path_dvdauthor[0],path_dvdauthor[1],'-x',os.path.join(getTempPath(),'dvdauthor.xml'))
    if result<>0:
        fatalError("Failed while running dvdauthor")
    write( "Finished  dvdauthor")

def CreateDVDISO():
    write("Creating ISO image")
    checkCancelFlag()
    result = os.spawnlp(os.P_WAIT, path_mkisofs[0], path_mkisofs[1], '-dvd-video', \
        '-V','MythTV BurnDVD','-o',os.path.join(getTempPath(),'mythburn.iso'), \
        os.path.join(getTempPath(),'dvd'))

    if result<>0:
        fatalError("Failed while running mkisofs.")

    write("Finished creating ISO image")

def BurnDVDISO():
    write( "Burning ISO image to %s" % dvddrivepath)
    checkCancelFlag()

    if mediatype == DVD_RW and erasedvdrw == True:
        command = path_growisofs[0] + " -use-the-force-luke -Z " + dvddrivepath + \
                  " -dvd-video -V 'MythTV BurnDVD' " + os.path.join(getTempPath(),'dvd')
    else:
        command = path_growisofs[0] + " -Z " + dvddrivepath + \
                  " -dvd-video -V 'MythTV BurnDVD' " + os.path.join(getTempPath(),'dvd')

    result = os.system(command)

    if result <> 0:
        write("-"*60)
        write("ERROR: Failed while running growisofs")
        write("Result %d, Command was: %s" % (result, command))
        write("Please check the troubleshooting section of the README for ways to fix this error")
        write("-"*60)
        write("")
        os.exit(2)

    os.spawnlp(os.P_WAIT, 'eject', 'eject', dvddrivepath)

    write("Finished burning ISO image")

def deMultiplexMPEG2File(folder, mediafile, video, audio1, audio2):
    checkCancelFlag()

    command = "mythreplex --demux  -o %s " % (folder + "/stream")
    command += "-v %d " % (video[VIDEO_ID] & 255)

    if audio1[AUDIO_ID] != -1: 
        if audio1[AUDIO_CODEC] == 'MP2':
            command += "-a %d " % (audio1[AUDIO_ID] & 255)
        elif audio1[AUDIO_CODEC] == 'AC3':
            command += "-c %d " % (audio1[AUDIO_ID] & 255)

    if audio2[AUDIO_ID] != -1: 
        if audio2[AUDIO_CODEC] == 'MP2':
            command += "-a %d " % (audio2[AUDIO_ID] & 255)
        elif audio2[AUDIO_CODEC] == 'AC3':
            command += "-c %d " % (audio2[AUDIO_ID] & 255)

    mediafile = quoteFilename(mediafile)
    command += mediafile
    write("Running: " + command)

    result = os.system(command)

    if result<>0:
        fatalError("Failed while running mythreplex. Command was %s" % command)

def runTcrequant(source,destination,percentage):
    checkCancelFlag()

    write (path_tcrequant[0] + " %s %s %s" % (source,destination,percentage))
    result=os.spawnlp(os.P_WAIT, path_tcrequant[0],path_tcrequant[1],
            "-i",source,
            "-o",destination,
            "-d","2",
            "-f","%s" % percentage)
    if result<>0:
        fatalError("Failed while running tcrequant")

def calculateFileSizes(files):
    """ Returns the sizes of all video, audio and menu files"""
    filecount=0
    totalvideosize=0
    totalaudiosize=0
    totalmenusize=0

    for node in files:
        filecount+=1
        #Generate a temp folder name for this file
        folder=getItemTempPath(filecount)
        #Process this file
        file=os.path.join(folder,"stream.mv2")
        #Get size of video in MBytes
        totalvideosize+=os.path.getsize(file) / 1024 / 1024
        #Get size of audio track 1
        totalaudiosize+=os.path.getsize(os.path.join(folder,"stream0.ac3")) / 1024 / 1024
        #Get size of audio track 2 if available 
        if doesFileExist(os.path.join(folder,"stream1.ac3")):
            totalaudiosize+=os.path.getsize(os.path.join(folder,"stream1.ac3")) / 1024 / 1024
        if doesFileExist(os.path.join(getTempPath(),"chaptermenu-%s.mpg" % filecount)):
            totalmenusize+=os.path.getsize(os.path.join(getTempPath(),"chaptermenu-%s.mpg" % filecount)) / 1024 / 1024

    filecount=1
    while doesFileExist(os.path.join(getTempPath(),"menu-%s.mpg" % filecount)):
        totalmenusize+=os.path.getsize(os.path.join(getTempPath(),"menu-%s.mpg" % filecount)) / 1024 / 1024
        filecount+=1

    return totalvideosize,totalaudiosize,totalmenusize

def performMPEG2Shrink(files,dvdrsize):
    checkCancelFlag()

    totalvideosize,totalaudiosize,totalmenusize=calculateFileSizes(files)

    #Report findings
    write( "Total size of video files, before multiplexing, is %s Mbytes, audio is %s MBytes, menus are %s MBytes." % (totalvideosize,totalaudiosize,totalmenusize))

    #Subtract the audio and menus from the size of the disk (we cannot shrink this further)
    dvdrsize-=totalaudiosize
    dvdrsize-=totalmenusize

    #Add a little bit for the multiplexing stream data
    totalvideosize=totalvideosize*1.05

    if dvdrsize<0:
        fatalError("Audio and menu files are greater than the size of a recordable DVD disk.  Giving up!")

    if totalvideosize>dvdrsize:
        write( "Need to shrink MPEG2 video files to fit onto recordable DVD, video is %s MBytes too big." % (totalvideosize - dvdrsize))
        scalepercentage=totalvideosize/dvdrsize
        write( "Need to scale by %s" % scalepercentage)

        if scalepercentage>3:
            write( "Large scale to shrink, may not work!")

        #tcrequant (transcode) is an optional install so may not be available
        if path_tcrequant[0] == "":
            fatalError("tcrequant is not available to resize the files.  Giving up!")

        filecount=0
        for node in files:
            filecount+=1
            runTcrequant(os.path.join(getItemTempPath(filecount),"stream.mv2"),os.path.join(getItemTempPath(filecount),"video.small.m2v"),scalepercentage)
            os.remove(os.path.join(getItemTempPath(filecount),"stream.mv2"))
            os.rename(os.path.join(getItemTempPath(filecount),"video.small.m2v"),os.path.join(getItemTempPath(filecount),"stream.mv2"))

        totalvideosize,totalaudiosize,totalmenusize=calculateFileSizes(files)        
        write( "Total DVD size AFTER TCREQUANT is %s MBytes" % (totalaudiosize + totalmenusize + (totalvideosize*1.05)))

    else:
        dvdrsize-=totalvideosize
        write( "Video will fit onto DVD. %s MBytes of space remaining on recordable DVD." % dvdrsize)


def createDVDAuthorXML(screensize, numberofitems):
    """Creates the xml file for dvdauthor to use the MythBurn menus."""

    #Get the main menu node (we must only have 1)
    menunode=themeDOM.getElementsByTagName("menu")
    if menunode.length!=1:
        fatalError("Cannot find the menu element in the theme file")
    menunode=menunode[0]

    menuitems=menunode.getElementsByTagName("item")
    #Total number of video items on a single menu page (no less than 1!)
    itemsperpage = menuitems.length
    write( "Menu items per page %s" % itemsperpage)

    if wantChapterMenu:
        #Get the chapter menu node (we must only have 1)
        submenunode=themeDOM.getElementsByTagName("submenu")
        if submenunode.length!=1:
            fatalError("Cannot find the submenu element in the theme file")

        submenunode=submenunode[0]

        chapteritems=submenunode.getElementsByTagName("chapter")
        #Total number of video items on a single menu page (no less than 1!)
        chapters = chapteritems.length
        write( "Chapters per recording %s" % chapters)

        del chapteritems
        del submenunode

    #Page number counter
    page=1

    #Item counter to indicate current video item
    itemnum=1

    write( "Creating DVD XML file for dvd author")

    dvddom = xml.dom.minidom.parseString(
                '''<dvdauthor>
                <vmgm>
                <menus lang="en">
                <pgc entry="title">
                </pgc>
                </menus>
                </vmgm>
                </dvdauthor>''')

    dvdauthor_element=dvddom.documentElement
    menus_element = dvdauthor_element.childNodes[1].childNodes[1]

    dvdauthor_element.insertBefore( dvddom.createComment("""
    DVD Variables
    g0=not used
    g1=not used
    g2=title number selected on current menu page (see g4)
    g3=1 if intro movie has played
    g4=last menu page on display
    """), dvdauthor_element.firstChild )
    dvdauthor_element.insertBefore(dvddom.createComment("dvdauthor XML file created by MythBurn script"), dvdauthor_element.firstChild )

    menus_element.appendChild( dvddom.createComment("Title menu used to hold intro movie") )

    dvdauthor_element.setAttribute("dest",os.path.join(getTempPath(),"dvd"))

    video = dvddom.createElement("video")
    video.setAttribute("format",videomode)
    menus_element.appendChild(video)

    pgc=menus_element.childNodes[1]

    if wantIntro:
        #code to skip over intro if its already played
        pre = dvddom.createElement("pre")
        pgc.appendChild(pre)
        vmgm_pre_node=pre
        del pre

        node = themeDOM.getElementsByTagName("intro")[0]
        introFile = node.attributes["filename"].value

        #Pick the correct intro movie based on video format ntsc/pal
        vob = dvddom.createElement("vob")
        vob.setAttribute("pause","")
        vob.setAttribute("file",os.path.join(getIntroPath(), videomode + '_' + introFile))
        pgc.appendChild(vob)
        del vob

        #We use g3 to indicate that the intro has been played at least once
        #default g2 to point to first recording
        post = dvddom.createElement("post")
        post .appendChild(dvddom.createTextNode("{g3=1;g2=1;jump menu 2;}"))
        pgc.appendChild(post)
        del post

    while itemnum <= numberofitems:
        write( "Menu page %s" % page)

        #For each menu page we need to create a new PGC structure
        menupgc = dvddom.createElement("pgc")
        menus_element.appendChild(menupgc)
        menupgc.setAttribute("pause","inf")

        menupgc.appendChild( dvddom.createComment("Menu Page %s" % page) )

        #Make sure the button last highlighted is selected
        #g4 holds the menu page last displayed
        pre = dvddom.createElement("pre")
        pre.appendChild(dvddom.createTextNode("{button=g2*1024;g4=%s;}" % page))
        menupgc.appendChild(pre)    

        vob = dvddom.createElement("vob")
        vob.setAttribute("file",os.path.join(getTempPath(),"menu-%s.mpg" % page))
        menupgc.appendChild(vob)    

        #Loop menu forever
        post = dvddom.createElement("post")
        post.appendChild(dvddom.createTextNode("jump cell 1;"))
        menupgc.appendChild(post)

        #Default settings for this page

        #Number of video items on this menu page
        itemsonthispage=0

        #Loop through all the items on this menu page
        while itemnum <= numberofitems and itemsonthispage < itemsperpage:
            menuitem=menuitems[ itemsonthispage ]

            itemsonthispage+=1

            #Get the XML containing information about this item
            infoDOM = xml.dom.minidom.parse( os.path.join(getItemTempPath(itemnum),"info.xml") )
            #Error out if its the wrong XML
            if infoDOM.documentElement.tagName != "fileinfo":
                fatalError("The info.xml file (%s) doesn't look right" % os.path.join(getItemTempPath(itemnum),"info.xml"))

            #write( themedom.toprettyxml())

            #Add this recording to this page's menu...
            button=dvddom.createElement("button")
            button.setAttribute("name","%s" % itemnum)
            button.appendChild(dvddom.createTextNode("{g2=" + "%s" % itemsonthispage + "; jump title %s;}" % itemnum))
            menupgc.appendChild(button)
            del button

            #Create a TITLESET for each item
            titleset = dvddom.createElement("titleset")
            dvdauthor_element.appendChild(titleset)

            #Comment XML file with title of video
            titleset.appendChild( dvddom.createComment( getText( infoDOM.getElementsByTagName("title")[0]) ) )

            menus= dvddom.createElement("menus")
            titleset.appendChild(menus)

            video = dvddom.createElement("video")
            video.setAttribute("format",videomode)
            menus.appendChild(video)

            if wantChapterMenu:
                mymenupgc = dvddom.createElement("pgc")
                menus.appendChild(mymenupgc)
                mymenupgc.setAttribute("pause","inf")

                pre = dvddom.createElement("pre")
                mymenupgc.appendChild(pre)
                if wantDetailsPage: 
                    pre.appendChild(dvddom.createTextNode("{button=s7 - 1 * 1024;}"))
                else:
                    pre.appendChild(dvddom.createTextNode("{button=s7 * 1024;}"))

                vob = dvddom.createElement("vob")
                vob.setAttribute("file",os.path.join(getTempPath(),"chaptermenu-%s.mpg" % itemnum))
                mymenupgc.appendChild(vob)    

                x=1
                while x<=chapters:
                    #Add this recording to this page's menu...
                    button=dvddom.createElement("button")
                    button.setAttribute("name","%s" % x)
                    if wantDetailsPage: 
                        button.appendChild(dvddom.createTextNode("jump title %s chapter %s;" % (1, x + 1)))
                    else:
                        button.appendChild(dvddom.createTextNode("jump title %s chapter %s;" % (1, x)))

                    mymenupgc.appendChild(button)
                    del button
                    x+=1

            titles = dvddom.createElement("titles")
            titleset.appendChild(titles)

            pgc = dvddom.createElement("pgc")
            titles.appendChild(pgc)
            #pgc.setAttribute("pause","inf")

            if wantDetailsPage:
                #add the detail page intro for this item
                vob = dvddom.createElement("vob")
                #vob.setAttribute("chapters",createVideoChapters(itemnum,chapters,getLengthOfVideo(itemnum),False) )
                vob.setAttribute("file",os.path.join(getTempPath(),"details-%s.mpg" % itemnum))
                pgc.appendChild(vob)

            vob = dvddom.createElement("vob")
            if wantChapterMenu:
                vob.setAttribute("chapters",createVideoChapters(itemnum,chapters,getLengthOfVideo(itemnum),False) )
            else:
                vob.setAttribute("chapters",createVideoChapters(itemnum,8,getLengthOfVideo(itemnum),False) )

            vob.setAttribute("file",os.path.join(getItemTempPath(itemnum),"final.mpg"))
            pgc.appendChild(vob)

            post = dvddom.createElement("post")
            post.appendChild(dvddom.createTextNode("call vmgm menu %s;" % (page + 1)))
            pgc.appendChild(post)

            #Quick variable tidy up (not really required under Python)
            del titleset
            del titles
            del menus
            del video
            del pgc
            del vob
            del post

            #Loop through all the nodes inside this menu item and pick previous / next buttons
            for node in menuitem.childNodes:

                if node.nodeName=="previous":
                    if page>1:
                        button=dvddom.createElement("button")
                        button.setAttribute("name","previous")
                        button.appendChild(dvddom.createTextNode("{g2=1;jump menu %s;}" % page ))
                        menupgc.appendChild(button)
                        del button


                elif node.nodeName=="next":
                    if itemnum < numberofitems:
                        button=dvddom.createElement("button")
                        button.setAttribute("name","next")
                        button.appendChild(dvddom.createTextNode("{g2=1;jump menu %s;}" % (page + 2)))
                        menupgc.appendChild(button)
                        del button

            #On to the next item
            itemnum+=1

        #Move on to the next page
        page+=1

    if wantIntro:
        #Menu creation is finished so we know how many pages were created
        #add to to jump to the correct one automatically
        dvdcode="if (g3 eq 1) {"
        while (page>1):
            page-=1;
            dvdcode+="if (g4 eq %s) " % page
            dvdcode+="jump menu %s;" % (page + 1)
            if (page>1):
                dvdcode+=" else "
        dvdcode+="}"       
        vmgm_pre_node.appendChild(dvddom.createTextNode(dvdcode))

    #Save xml to file
    WriteXMLToFile (dvddom,os.path.join(getTempPath(),"dvdauthor.xml"))

    #Destroy the DOM and free memory
    dvddom.unlink()   

def createDVDAuthorXMLNoMainMenu(screensize, numberofitems):
    """Creates the xml file for dvdauthor to use the MythBurn menus."""

    # creates a simple DVD with only a chapter menus shown before each video
    # can contain an intro movie and each title can have a details page
    # displayed before each title

    write( "Creating DVD XML file for dvd author (No Main Menu)")
    #FIXME:
    assert False

def createDVDAuthorXMLNoMenus(screensize, numberofitems):
    """Creates the xml file for dvdauthor containing no menus."""

    # creates a simple DVD with no menus that chains the videos one after the other
    # can contain an intro movie and each title can have a details page
    # displayed before each title

    write( "Creating DVD XML file for dvd author (No Menus)")

    dvddom = xml.dom.minidom.parseString(
                '''
                <dvdauthor>
                    <vmgm>
                    </vmgm>
                </dvdauthor>''')

    dvdauthor_element = dvddom.documentElement
    titleset = dvddom.createElement("titleset")
    titles = dvddom.createElement("titles")
    titleset.appendChild(titles)
    dvdauthor_element.appendChild(titleset)

    dvdauthor_element.insertBefore(dvddom.createComment("dvdauthor XML file created by MythBurn script"), dvdauthor_element.firstChild )
    dvdauthor_element.setAttribute("dest",os.path.join(getTempPath(),"dvd"))

    fileCount = 0
    itemNum = 1

    if wantIntro:
        node = themeDOM.getElementsByTagName("intro")[0]
        introFile = node.attributes["filename"].value

        titles.appendChild(dvddom.createComment("Intro movie"))
        pgc = dvddom.createElement("pgc")
        vob = dvddom.createElement("vob")
        vob.setAttribute("file",os.path.join(getIntroPath(), videomode + '_' + introFile))
        pgc.appendChild(vob)
        titles.appendChild(pgc)
        post = dvddom.createElement("post")
        post .appendChild(dvddom.createTextNode("jump title 2 chapter 1;"))
        pgc.appendChild(post)
        titles.appendChild(pgc)
        fileCount +=1
        del pgc
        del vob
        del post


    while itemNum <= numberofitems:
        write( "Adding item %s" % itemNum)

        pgc = dvddom.createElement("pgc")

        if wantDetailsPage:
            #add the detail page intro for this item
            vob = dvddom.createElement("vob")
            #vob.setAttribute("chapters",createVideoChapters(itemnum,chapters,getLengthOfVideo(itemnum),False) )
            vob.setAttribute("file",os.path.join(getTempPath(),"details-%s.mpg" % itemNum))
            pgc.appendChild(vob)
            fileCount +=1
            del vob

        vob = dvddom.createElement("vob")
        vob.setAttribute("file", os.path.join(getItemTempPath(itemNum), "final.mpg"))
        pgc.appendChild(vob)
        del vob

        post = dvddom.createElement("post")
        if itemNum == numberofitems:
            post.appendChild(dvddom.createTextNode("exit;"))
        else:
            post.appendChild(dvddom.createTextNode("jump title %s chapter 1;" % itemNum + 1))

        pgc.appendChild(post)
        fileCount +=1

        titles.appendChild(pgc)
        del pgc

        itemNum +=1

    #Save xml to file
    WriteXMLToFile (dvddom,os.path.join(getTempPath(),"dvdauthor.xml"))

    #Destroy the DOM and free memory
    dvddom.unlink()

def drawThemeItem(page, itemsonthispage, itemnum, menuitem, bgimage, draw, bgimagemask, drawmask, spumuxdom, spunode, numberofitems, chapternumber, chapterlist):
    """Draws text and graphics onto a dvd menu, called by createMenu and createChapterMenu"""
    #Get the XML containing information about this item
    infoDOM = xml.dom.minidom.parse( os.path.join(getItemTempPath(itemnum),"info.xml") )
    #Error out if its the wrong XML
    if infoDOM.documentElement.tagName != "fileinfo":
        fatalError("The info.xml file (%s) doesn't look right" % os.path.join(getItemTempPath(itemnum),"info.xml"))

    #boundarybox holds the max and min dimensions for this item so we can auto build a menu highlight box
    boundarybox=9999,9999,0,0
    wantHighlightBox = True

    #Loop through all the nodes inside this menu item
    for node in menuitem.childNodes:

        #Process each type of item to add it onto the background image
        if node.nodeName=="graphic":
            #Overlay graphic image onto background
            imagefilename = expandItemText(infoDOM,node.attributes["filename"].value, itemnum, page, itemsonthispage, chapternumber, chapterlist)

            if doesFileExist(imagefilename):
                picture = Image.open(imagefilename,"r").resize((int(node.attributes["w"].value), int(node.attributes["h"].value)))
                picture = picture.convert("RGBA")
                bgimage.paste(picture,(int(node.attributes["x"].value), int(node.attributes["y"].value)),picture)
                del picture
                write( "Added image %s" % imagefilename)

                boundarybox=checkBoundaryBox(boundarybox, node)
            else:
                write( "Image file does not exist '%s'" % imagefilename)

        elif node.nodeName=="text":
            #Apply some text to the background, including wordwrap if required.
            text=expandItemText(infoDOM,node.attributes["value"].value, itemnum, page, itemsonthispage,chapternumber,chapterlist)
            if text>"":
                paintText( draw,
                           int(node.attributes["x"].value),
                           int(node.attributes["y"].value),
                           int(node.attributes["w"].value),
                           int(node.attributes["h"].value),
                           text,
                           themeFonts[int(node.attributes["font"].value)],
                           node.attributes["colour"].value,
                           node.attributes["align"].value )
                boundarybox=checkBoundaryBox(boundarybox, node)
            del text

        elif node.nodeName=="previous":
            if page>1:
                #Overlay previous graphic button onto background
                imagefilename = expandItemText(infoDOM,node.attributes["filename"].value, itemnum, page, itemsonthispage,chapternumber,chapterlist)
                if not doesFileExist(imagefilename):
                    fatalError("Cannot find image for previous button (%s)." % imagefilename)
                maskimagefilename = expandItemText(infoDOM,node.attributes["mask"].value, itemnum, page, itemsonthispage,chapternumber,chapterlist)
                if not doesFileExist(maskimagefilename):
                    fatalError("Cannot find mask image for previous button (%s)." % maskimagefilename)

                picture=Image.open(imagefilename,"r").resize((int(node.attributes["w"].value), int(node.attributes["h"].value)))
                picture=picture.convert("RGBA")
                bgimage.paste(picture,(int(node.attributes["x"].value), int(node.attributes["y"].value)),picture)
                del picture
                write( "Added previous button image %s" % imagefilename)

                picture=Image.open(maskimagefilename,"r").resize((int(node.attributes["w"].value), int(node.attributes["h"].value)))
                picture=picture.convert("RGBA")
                bgimagemask.paste(picture,(int(node.attributes["x"].value), int(node.attributes["y"].value)),picture)
                del picture
                write( "Added previous button mask image %s" % imagefilename)

                button = spumuxdom.createElement("button")
                button.setAttribute("name","previous")
                button.setAttribute("x0","%s" % int(node.attributes["x"].value))
                button.setAttribute("y0","%s" % int(node.attributes["y"].value))
                button.setAttribute("x1","%s" % (int(node.attributes["x"].value)+int(node.attributes["w"].value)))
                button.setAttribute("y1","%s" % (int(node.attributes["y"].value)+int(node.attributes["h"].value)))
                spunode.appendChild(button)


        elif node.nodeName=="next":
            if itemnum < numberofitems:
                #Overlay next graphic button onto background
                imagefilename = expandItemText(infoDOM,node.attributes["filename"].value, itemnum, page, itemsonthispage,chapternumber,chapterlist)
                if not doesFileExist(imagefilename):
                    fatalError("Cannot find image for next button (%s)." % imagefilename)
                maskimagefilename = expandItemText(infoDOM,node.attributes["mask"].value, itemnum, page, itemsonthispage,chapternumber,chapterlist)
                if not doesFileExist(maskimagefilename):
                    fatalError("Cannot find mask image for next button (%s)." % maskimagefilename)

                picture = Image.open(imagefilename,"r").resize((int(node.attributes["w"].value), int(node.attributes["h"].value)))
                picture = picture.convert("RGBA")
                bgimage.paste(picture,(int(node.attributes["x"].value), int(node.attributes["y"].value)),picture)
                del picture
                write( "Added next button image %s " % imagefilename)

                picture=Image.open(maskimagefilename,"r").resize((int(node.attributes["w"].value), int(node.attributes["h"].value)))
                picture=picture.convert("RGBA")
                bgimagemask.paste(picture,(int(node.attributes["x"].value), int(node.attributes["y"].value)),picture)
                del picture
                write( "Added next button mask image %s" % imagefilename)

                button = spumuxdom.createElement("button")
                button.setAttribute("name","next")
                button.setAttribute("x0","%s" % int(node.attributes["x"].value))
                button.setAttribute("y0","%s" % int(node.attributes["y"].value))
                button.setAttribute("x1","%s" % (int(node.attributes["x"].value) + int(node.attributes["w"].value)))
                button.setAttribute("y1","%s" % (int(node.attributes["y"].value) + int(node.attributes["h"].value)))
                spunode.appendChild(button)

        elif node.nodeName=="button":
            wantHighlightBox = False

            #Overlay item graphic/text button onto background
            imagefilename = expandItemText(infoDOM,node.attributes["filename"].value, itemnum, page, itemsonthispage,chapternumber,chapterlist)
            if not doesFileExist(imagefilename):
                    fatalError("Cannot find image for menu button (%s)." % imagefilename)
            maskimagefilename = expandItemText(infoDOM,node.attributes["mask"].value, itemnum, page, itemsonthispage,chapternumber,chapterlist)
            if not doesFileExist(maskimagefilename):
                    fatalError("Cannot find mask image for menu button (%s)." % maskimagefilename)

            picture=Image.open(imagefilename,"r").resize((int(node.attributes["w"].value), int(node.attributes["h"].value)))
            picture=picture.convert("RGBA")
            bgimage.paste(picture,(int(node.attributes["x"].value), int(node.attributes["y"].value)),picture)
            del picture

            # if we have some text paint that over the image
            textnode = node.getElementsByTagName("textnormal")
            if textnode.length > 0:
                textnode = textnode[0]
                text=expandItemText(infoDOM,textnode.attributes["value"].value, itemnum, page, itemsonthispage,chapternumber,chapterlist)
                if text>"":
                    paintText( draw,
                           int(textnode.attributes["x"].value),
                           int(textnode.attributes["y"].value),
                           int(textnode.attributes["w"].value),
                           int(textnode.attributes["h"].value),
                           text,
                           themeFonts[int(textnode.attributes["font"].value)],
                           textnode.attributes["colour"].value,
                           textnode.attributes["align"].value )
                boundarybox=checkBoundaryBox(boundarybox, node)
                del text

            write( "Added button image %s" % imagefilename)

            picture=Image.open(maskimagefilename,"r").resize((int(node.attributes["w"].value), int(node.attributes["h"].value)))
            picture=picture.convert("RGBA")
            bgimagemask.paste(picture,(int(node.attributes["x"].value), int(node.attributes["y"].value)),picture)
            #del picture

            # if we have some text paint that over the image
            textnode = node.getElementsByTagName("textselected")
            if textnode.length > 0:
                textnode = textnode[0]
                text=expandItemText(infoDOM,textnode.attributes["value"].value, itemnum, page, itemsonthispage,chapternumber,chapterlist)
                textImage=Image.new("1",picture.size)
                textDraw=ImageDraw.Draw(textImage)

                if text>"":
                    paintText(textDraw,
                           int(node.attributes["x"].value) - int(textnode.attributes["x"].value),
                           int(node.attributes["y"].value) - int(textnode.attributes["y"].value),
                           int(textnode.attributes["w"].value),
                           int(textnode.attributes["h"].value),
                           text,
                           themeFonts[int(textnode.attributes["font"].value)],
                           "white",
                           textnode.attributes["align"].value )
                bgimagemask.paste(textnode.attributes["colour"].value, (int(textnode.attributes["x"].value), int(textnode.attributes["y"].value)),textImage)
#               bgimagemask.paste(textImage, (int(textnode.attributes["x"].value), int(textnode.attributes["y"].value)),textImage)
                boundarybox=checkBoundaryBox(boundarybox, node)
                textImage.save(os.path.join(getTempPath(),"textimage-%s.png" % itemnum),"PNG",quality=99,optimize=0)

                del text, textImage, textDraw
            del picture

        elif node.nodeName=="#text":
            #Do nothing
            assert True
        else:
            write( "Dont know how to process %s" % node.nodeName)

    if drawmask == None:
        return

    #Draw the mask for this item

    if wantHighlightBox == True:
        #Make the boundary box bigger than the content to avoid over write(ing (2 pixels)
        boundarybox=boundarybox[0]-1,boundarybox[1]-1,boundarybox[2]+1,boundarybox[3]+1
        #draw.rectangle(boundarybox,outline="white")
        drawmask.rectangle(boundarybox,outline="red")

        #Draw another line to make the box thicker - PIL does not support linewidth
        boundarybox=boundarybox[0]-1,boundarybox[1]-1,boundarybox[2]+1,boundarybox[3]+1
        #draw.rectangle(boundarybox,outline="white")
        drawmask.rectangle(boundarybox,outline="red")

    node = spumuxdom.createElement("button")
    #Fiddle this for chapter marks....
    if chapternumber>0:
        node.setAttribute("name","%s" % chapternumber)
    else:
        node.setAttribute("name","%s" % itemnum)
    node.setAttribute("x0","%d" % int(boundarybox[0]))
    node.setAttribute("y0","%d" % int(boundarybox[1]))
    node.setAttribute("x1","%d" % int(boundarybox[2] + 1))
    node.setAttribute("y1","%d" % int(boundarybox[3] + 1))
    spunode.appendChild(node)   

def createMenu(screensize, numberofitems):
    """Creates all the necessary menu images and files for the MythBurn menus."""

    #Get the main menu node (we must only have 1)
    menunode=themeDOM.getElementsByTagName("menu")
    if menunode.length!=1:
        fatalError("Cannot find menu element in theme file")
    menunode=menunode[0]

    menuitems=menunode.getElementsByTagName("item")
    #Total number of video items on a single menu page (no less than 1!)
    itemsperpage = menuitems.length
    write( "Menu items per page %s" % itemsperpage)

    #Get background image filename
    backgroundfilename = menunode.attributes["background"].value
    if backgroundfilename=="":
        fatalError("Background image is not set in theme file")

    backgroundfilename = getThemeFile(themeName,backgroundfilename)
    write( "Background image file is %s" % backgroundfilename)
    if not doesFileExist(backgroundfilename):
        fatalError("Background image not found (%s)" % backgroundfilename)


    #Page number counter
    page=1

    #Item counter to indicate current video item
    itemnum=1

    write( "Creating DVD menus")

    while itemnum <= numberofitems:
        write( "Menu page %s" % page)

        #Default settings for this page

        #Number of video items on this menu page
        itemsonthispage=0

        #Load background image
        bgimage=Image.open(backgroundfilename,"r").resize(screensize)
        draw=ImageDraw.Draw(bgimage)

        #Create image to hold button masks (same size as background)
        bgimagemask=Image.new("RGBA",bgimage.size)
        drawmask=ImageDraw.Draw(bgimagemask)

        spumuxdom = xml.dom.minidom.parseString('<subpictures><stream><spu force="yes" start="00:00:00.0" highlight="" select="" ></spu></stream></subpictures>')
        spunode = spumuxdom.documentElement.firstChild.firstChild

        #Loop through all the items on this menu page
        while itemnum <= numberofitems and itemsonthispage < itemsperpage:
            menuitem=menuitems[ itemsonthispage ]

            itemsonthispage+=1

            drawThemeItem(page, itemsonthispage,
                        itemnum, menuitem, bgimage,
                        draw, bgimagemask, drawmask,
                        spumuxdom, spunode, numberofitems, 0,"")

            #On to the next item
            itemnum+=1

        #Save this menu image and its mask
        bgimage.save(os.path.join(getTempPath(),"background-%s.png" % page),"PNG",quality=99,optimize=0)
        bgimagemask.save(os.path.join(getTempPath(),"backgroundmask-%s.png" % page),"PNG",quality=99,optimize=0)

## Experimental!
##        for i in range(1,750):
##            bgimage.save(os.path.join(getTempPath(),"background-%s-%s.ppm" % (page,i)),"PPM",quality=99,optimize=0)

        spumuxdom.documentElement.firstChild.firstChild.setAttribute("select",os.path.join(getTempPath(),"backgroundmask-%s.png" % page))
        spumuxdom.documentElement.firstChild.firstChild.setAttribute("highlight",os.path.join(getTempPath(),"backgroundmask-%s.png" % page))

        #Release large amounts of memory ASAP !
        del draw
        del bgimage
        del drawmask
        del bgimagemask

        #write( spumuxdom.toprettyxml())
        WriteXMLToFile (spumuxdom,os.path.join(getTempPath(),"spumux-%s.xml" % page))

        write("Encoding Menu Page %s" % page)
        encodeMenu(os.path.join(getTempPath(),"background-%s.png" % page),
                    os.path.join(getTempPath(),"temp.m2v"),
                    getThemeFile(themeName,"menumusic.mp2"),
                    os.path.join(getTempPath(),"temp.mpg"),
                    os.path.join(getTempPath(),"spumux-%s.xml" % page),
                    os.path.join(getTempPath(),"menu-%s.mpg" % page))

        #Tidy up temporary files
####        os.remove(os.path.join(getTempPath(),"spumux-%s.xml" % page))
####        os.remove(os.path.join(getTempPath(),"background-%s.png" % page))
####        os.remove(os.path.join(getTempPath(),"backgroundmask-%s.png" % page))

        #Move on to the next page
        page+=1

def createChapterMenu(screensize, numberofitems):
    """Creates all the necessary menu images and files for the MythBurn menus."""

    #Get the main menu node (we must only have 1)
    menunode=themeDOM.getElementsByTagName("submenu")
    if menunode.length!=1:
        fatalError("Cannot find submenu element in theme file")
    menunode=menunode[0]

    menuitems=menunode.getElementsByTagName("chapter")
    #Total number of video items on a single menu page (no less than 1!)
    itemsperpage = menuitems.length
    write( "Chapter items per page %s " % itemsperpage)

    #Get background image filename
    backgroundfilename = menunode.attributes["background"].value
    if backgroundfilename=="":
        fatalError("Background image is not set in theme file")
    backgroundfilename = getThemeFile(themeName,backgroundfilename)
    write( "Background image file is %s" % backgroundfilename)
    if not doesFileExist(backgroundfilename):
        fatalError("Background image not found (%s)" % backgroundfilename)

    #Page number counter
    page=1

    write( "Creating DVD sub-menus")

    while page <= numberofitems:
        write( "Sub-menu %s " % page)

        #Default settings for this page

        #Load background image
        bgimage=Image.open(backgroundfilename,"r").resize(screensize)
        draw=ImageDraw.Draw(bgimage)

        #Create image to hold button masks (same size as background)
        bgimagemask=Image.new("RGBA",bgimage.size)
        drawmask=ImageDraw.Draw(bgimagemask)

        spumuxdom = xml.dom.minidom.parseString('<subpictures><stream><spu force="yes" start="00:00:00.0" highlight="" select="" ></spu></stream></subpictures>')
        spunode = spumuxdom.documentElement.firstChild.firstChild

        #Extracting the thumbnails for the video takes an incredibly long time
        #need to look at a switch to disable this. or not use FFMPEG
        chapterlist=createVideoChapters(page,itemsperpage,getLengthOfVideo(page),True)
        chapterlist=string.split(chapterlist,",")

        #Loop through all the items on this menu page
        chapter=0
        while chapter < itemsperpage:  # and itemsonthispage < itemsperpage:
            menuitem=menuitems[ chapter ]
            chapter+=1

            drawThemeItem(page, itemsperpage, page, menuitem, 
                        bgimage, draw, 
                        bgimagemask, drawmask, 
                        spumuxdom, spunode, 
                        999, chapter, chapterlist)

        #Save this menu image and its mask
        bgimage.save(os.path.join(getTempPath(),"chaptermenu-%s.png" % page),"PNG",quality=99,optimize=0)

        bgimagemask.save(os.path.join(getTempPath(),"chaptermenumask-%s.png" % page),"PNG",quality=99,optimize=0)

        spumuxdom.documentElement.firstChild.firstChild.setAttribute("select",os.path.join(getTempPath(),"chaptermenumask-%s.png" % page))
        spumuxdom.documentElement.firstChild.firstChild.setAttribute("highlight",os.path.join(getTempPath(),"chaptermenumask-%s.png" % page))

        #Release large amounts of memory ASAP !
        del draw
        del bgimage
        del drawmask
        del bgimagemask

        #write( spumuxdom.toprettyxml())
        WriteXMLToFile (spumuxdom,os.path.join(getTempPath(),"chapterspumux-%s.xml" % page))

        write("Encoding Chapter Menu Page %s" % page)
        encodeMenu(os.path.join(getTempPath(),"chaptermenu-%s.png" % page),
                    os.path.join(getTempPath(),"temp.m2v"),
                    getThemeFile(themeName,"menumusic.mp2"),
                    os.path.join(getTempPath(),"temp.mpg"),
                    os.path.join(getTempPath(),"chapterspumux-%s.xml" % page),
                    os.path.join(getTempPath(),"chaptermenu-%s.mpg" % page))

        #Tidy up
####        os.remove(os.path.join(getTempPath(),"chaptermenu-%s.png" % page))
####        os.remove(os.path.join(getTempPath(),"chaptermenumask-%s.png" % page))
####        os.remove(os.path.join(getTempPath(),"chapterspumux-%s.xml" % page))

        #Move on to the next page
        page+=1

def createDetailsPage(screensize, numberofitems):
    """Creates all the necessary images and files for the details page."""

    write( "Creating details pages")

    #Get the detailspage node (we must only have 1)
    detailnode=themeDOM.getElementsByTagName("detailspage")
    if detailnode.length!=1:
        fatalError("Cannot find detailspage element in theme file")
    detailnode=detailnode[0]

    #Get background image filename
    backgroundfilename = detailnode.attributes["background"].value
    if backgroundfilename=="":
        fatalError("Background image is not set in theme file")
    backgroundfilename = getThemeFile(themeName,backgroundfilename)
    write( "Background image file is %s" % backgroundfilename)
    if not doesFileExist(backgroundfilename):
        fatalError("Background image not found (%s)" % backgroundfilename)

    #Item counter to indicate current video item
    itemnum=1

    while itemnum <= numberofitems:
        write( "Creating details page for %s" % itemnum)

        #Load background image
        bgimage=Image.open(backgroundfilename,"r").resize(screensize)
        draw=ImageDraw.Draw(bgimage)

        spumuxdom = xml.dom.minidom.parseString('<subpictures><stream><spu force="yes" start="00:00:00.0" highlight="" select="" ></spu></stream></subpictures>')
        spunode = spumuxdom.documentElement.firstChild.firstChild

        drawThemeItem(0, 0, itemnum, detailnode, bgimage, draw, None, None,
                      spumuxdom, spunode, numberofitems, 0, "")

        #Save this details image
        bgimage.save(os.path.join(getTempPath(),"details-%s.png" % itemnum),"PNG",quality=99,optimize=0)

        #Release large amounts of memory ASAP !
        del draw
        del bgimage

        #write( spumuxdom.toprettyxml())
        WriteXMLToFile (spumuxdom,os.path.join(getTempPath(),"detailsspumux-%s.xml" % itemnum))

        write("Encoding Details Page %s" % itemnum)
        encodeMenu(os.path.join(getTempPath(),"details-%s.png" % itemnum),
                    os.path.join(getTempPath(),"temp.m2v"),
                    getThemeFile(themeName,"menumusic.mp2"),
                    os.path.join(getTempPath(),"temp.mpg"),
                    os.path.join(getTempPath(),"detailsspumux-%s.xml" % itemnum),
                    os.path.join(getTempPath(),"details-%s.mpg" % itemnum))

        #On to the next item
        itemnum+=1

def isMediaAVIFile(file):
    fh = open(file, 'rb')
    Magic = fh.read(4)
    fh.close()
    return Magic=="RIFF"

def processAudio(folder):
    """encode audio to ac3 for better compression and compatability with NTSC players"""

    # process track 1
    if not encodetoac3 and doesFileExist(os.path.join(folder,'stream0.mp2')):
        #don't re-encode to ac3 if the user doesn't want it
        os.rename(os.path.join(folder,'stream0.mp2'), os.path.join(folder,'stream0.ac3'))
    elif doesFileExist(os.path.join(folder,'stream0.mp2'))==True:
        write( "Audio track 1 is in mp2 format - re-encoding to ac3")
        encodeAudio("ac3",os.path.join(folder,'stream0.mp2'), os.path.join(folder,'stream0.ac3'),True)
    elif doesFileExist(os.path.join(folder,'stream0.mpa'))==True:
        write( "Audio track 1 is in mpa format - re-encoding to ac3")
        encodeAudio("ac3",os.path.join(folder,'stream0.mpa'), os.path.join(folder,'stream0.ac3'),True)
    elif doesFileExist(os.path.join(folder,'stream0.ac3'))==True:
        write( "Audio is already in ac3 format")
    elif doesFileExist(os.path.join(folder,'stream0.ac3'))==True:
        write( "Audio is already in ac3 format")
    else:
        fatalError("Track 1 - Unknown audio format or de-multiplex failed!")

    # process track 2
    if not encodetoac3 and doesFileExist(os.path.join(folder,'stream1.mp2')):
        #don't re-encode to ac3 if the user doesn't want it
        os.rename(os.path.join(folder,'stream1.mp2'), os.path.join(folder,'stream1.ac3'))
    elif doesFileExist(os.path.join(folder,'stream1.mp2'))==True:
        write( "Audio track 2 is in mp2 format - re-encoding to ac3")
        encodeAudio("ac3",os.path.join(folder,'stream1.mp2'), os.path.join(folder,'stream1.ac3'),True)
    elif doesFileExist(os.path.join(folder,'stream1.mpa'))==True:
        write( "Audio track 2 is in mpa format - re-encoding to ac3")
        encodeAudio("ac3",os.path.join(folder,'stream1.mpa'), os.path.join(folder,'stream1.ac3'),True)
    elif doesFileExist(os.path.join(folder,'stream1.ac3'))==True:
        write( "Audio is already in ac3 format")
    elif doesFileExist(os.path.join(folder,'stream1.ac3'))==True:
        write( "Audio is already in ac3 format")


# tuple index constants
VIDEO_INDEX = 0
VIDEO_CODEC = 1
VIDEO_ID    = 2

AUDIO_INDEX = 0
AUDIO_CODEC = 1
AUDIO_ID    = 2
AUDIO_LANG  = 3

def selectStreams(folder):
    """Choose the streams we want from the source file"""

    video    = (-1, 'N/A', -1)         # index, codec, ID
    audio1   = (-1, 'N/A', -1, 'N/A')  # index, codec, ID, lang
    audio2   = (-1, 'N/A', -1, 'N/A')

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(folder, 'streaminfo.xml'))
    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("This does not look like a stream info file (%s)" % os.path.join(folder, 'streaminfo.xml'))


    #get video ID, CODEC
    nodes = infoDOM.getElementsByTagName("video")
    if nodes.length == 0:
        write("Didn't find any video elements in stream info file.!!!")
        write("");
        os.exit(1)
    if nodes.length > 1:
        write("Found more than one video element in stream info file.!!!")
    node = nodes[0]
    video = (int(node.attributes["streamindex"].value), node.attributes["codec"].value, int(node.attributes["id"].value))

    #get audioID's - we choose the best 2 audio streams using this algorithm
    # 1. if there is one or more stream(s) using the 1st preferred language we use that
    # 2. if there is one or more stream(s) using the 2nd preferred language we use that
    # 3. if we still haven't found a stream we use the stream with the lowest PID
    # 4. we prefer ac3 over mp2
    # 5. if there are more that one stream with the chosen language we use the one with the lowest PID

    write("Preferred audio languages %s and %s" % (preferredlang1, preferredlang2))

    nodes = infoDOM.getElementsByTagName("audio")

    if nodes.length == 0:
        write("Didn't find any audio elements in stream info file.!!!")
        write("");
        os.exit(1)

    found = False
    # first try to find a stream with ac3 and preferred language 1
    for node in nodes:
        index = int(node.attributes["streamindex"].value)
        lang = node.attributes["language"].value
        format = string.upper(node.attributes["codec"].value)
        pid = int(node.attributes["id"].value)
        if lang == preferredlang1 and format == "AC3":
            if found:
                if pid < audio1[AUDIO_ID]:
                    audio1 = (index, format, pid, lang)
            else:
                audio1 = (index, format, pid, lang)
            found = True

    # second try to find a stream with mp2 and preferred language 1
    if not found:
        for node in nodes:
            index = int(node.attributes["streamindex"].value)
            lang = node.attributes["language"].value
            format = string.upper(node.attributes["codec"].value)
            pid = int(node.attributes["id"].value)
            if lang == preferredlang1 and format == "MP2":
                if found:
                    if pid < audio1[AUDIO_ID]:
                        audio1 = (index, format, pid, lang)
                else:
                    audio1 = (index, format, pid, lang)
                found = True

    # finally use the stream with the lowest pid, prefer ac3 over mp2
    if not found:
        for node in nodes:
            int(node.attributes["streamindex"].value)
            format = string.upper(node.attributes["codec"].value)
            pid = int(node.attributes["id"].value)
            if not found:
                audio1 = (index, format, pid, lang)
            else:
                if format == "AC3" and audio1[1] == "MP2":
                    audio1 = (index, format, pid, lang)
                else:
                    if pid < audio1[AUDIO_ID]:
                        audio1 = (index, format, pid, lang)
            found = True

    # do we need to find a second audio stream?
    if preferredlang1 != preferredlang2 and nodes.length > 1:
        found = False
        # first try to find a stream with ac3 and preferred language 2
        for node in nodes:
            index = int(node.attributes["streamindex"].value)
            lang = node.attributes["language"].value
            format = string.upper(node.attributes["codec"].value)
            pid = int(node.attributes["id"].value)
            if lang == preferredlang2 and format == "AC3":
                if found:
                    if pid < audio2[AUDIO_ID]:
                        audio2 = (index, format, pid, lang)
                else:
                    audio2 = (index, format, pid, lang)
                found = True

        # second try to find a stream with mp2 and preferred language 2
        if not found:
            for node in nodes:
                index = int(node.attributes["streamindex"].value)
                lang = node.attributes["language"].value
                format = string.upper(node.attributes["codec"].value)
                pid = int(node.attributes["id"].value)
                if lang == preferredlang2 and format == "MP2":
                    if found:
                        if pid < audio2[AUDIO_ID]:
                            audio2 = (index, format, pid, lang)
                    else:
                        audio2 = (index, format, pid, lang)
                    found = True

        # finally use the stream with the lowest pid, prefer ac3 over mp2
        if not found:
            for node in nodes:
                index = int(node.attributes["streamindex"].value)
                format = string.upper(node.attributes["codec"].value)
                pid = int(node.attributes["id"].value)
                if not found:
                    # make sure we don't choose the same stream as audio1
                    if pid != audio1[AUDIO_ID]:
                        audio2 = (index, format, pid, lang)
                        found = True
                else:
                    if format == "AC3" and audio2[AUDIO_CODEC] == "MP2" and pid != audio1[AUDIO_ID]:
                        audio2 = (index, format, pid, lang)
                    else:
                        if pid < audio2[AUDIO_ID] and pid != audio1[AUDIO_ID]:
                            audio2 = (index, format, pid, lang)


    write("Video id: %x, Audio1: %x (%s, %s), Audio2: %x (%s, %s)" % \
        (video[VIDEO_ID], audio1[AUDIO_ID], audio1[AUDIO_CODEC], audio1[AUDIO_LANG], \
         audio2[AUDIO_ID], audio2[AUDIO_CODEC], audio2[AUDIO_LANG]))

    return (video, audio1, audio2)

def selectAspectRatio(folder):
    """figure out what aspect ratio we want from the source file"""

    #this should be smarter and look though the file for any AR changes
    #at the moment it just uses the AR found at the start of the file

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(folder, 'streaminfo.xml'))
    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("This does not look like a stream info file (%s)" % os.path.join(folder, 'streaminfo.xml'))


    #get aspect ratio
    nodes = infoDOM.getElementsByTagName("video")
    if nodes.length == 0:
        write("Didn't find any video elements in stream info file.!!!")
        write("");
        os.exit(1)
    if nodes.length > 1:
        write("Found more than one video element in stream info file.!!!")
    node = nodes[0]
    try:
        ar = float(node.attributes["aspectratio"].value)
        if ar > float(4.0/3.0 - 0.00001) and ar < float(4.0/3.0 + 0.00001):
            aspectratio = "4:3"
            write("Aspect ratio is 4:3")
        elif ar > float(16.0/9.0 - 0.00001) and ar < float(16.0/9.0 + 0.00001):
            aspectratio = "16:9"
            write("Aspect ratio is 16:9")
        else:
            write("Unknown aspect ratio %f - Using 16:9" % ar)
            aspectratio = "16:9"
    except:
        aspectratio = "16/9"

    return aspectratio

def getVideoCodec(folder):
    """Get the video codec from the streaminfo.xml for the file"""

    #open the XML containing information about this file
    infoDOM = xml.dom.minidom.parse(os.path.join(folder, 'streaminfo.xml'))
    #error out if its the wrong XML
    if infoDOM.documentElement.tagName != "file":
        fatalError("This does not look like a stream info file (%s)" % os.path.join(folder, 'streaminfo.xml'))

    nodes = infoDOM.getElementsByTagName("video")
    if nodes.length == 0:
        write("Didn't find any video elements in stream info file!!!")
        write("");
        os.exit(1)
    if nodes.length > 1:
        write("Found more than one video element in stream info file!!!")
    node = nodes[0]
    return node.attributes["codec"].value

def isFileOkayForDVD(folder):
    """return true if the file is dvd compliant"""

    if string.lower(getVideoCodec(folder)) != "mpeg2video":
        return False

#    if string.lower(getAudioCodec(folder)) != "ac3" and encodeToAC3:
#        return False

    videosize = getVideoSize(os.path.join(folder, "streaminfo.xml"))

    if not isResolutionOkayForDVD(videosize):
        return False

    return True

def processFile(file, folder):
    """Process a single video/recording file ready for burning."""

    write( "*************************************************************")
    write( "Processing file " + file.attributes["filename"].value + " of type " + file.attributes["type"].value)
    write( "*************************************************************")

    #As part of this routine we need to pre-process the video this MAY mean:
    #1. removing commercials/cleaning up mpeg2 stream
    #2. encoding to mpeg2 (if its an avi for instance or isn't DVD compatible)
    #3. selecting audio track to use and encoding audio from mp2 into ac3
    #4. de-multiplexing into video and audio steams)

    mediafile=""

    if file.attributes["type"].value=="recording":
        mediafile=os.path.join(recordingpath, file.attributes["filename"].value)
    elif file.attributes["type"].value=="video":
        mediafile=os.path.join(videopath, file.attributes["filename"].value)
    elif file.attributes["type"].value=="file":
        mediafile=file.attributes["filename"].value
    else:
        fatalError("Unknown type of video file it must be 'recording', 'video' or 'file'.")

    #Get the XML containing information about this item
    infoDOM = xml.dom.minidom.parse( os.path.join(folder,"info.xml") )
    #Error out if its the wrong XML
    if infoDOM.documentElement.tagName != "fileinfo":
        fatalError("The info.xml file (%s) doesn't look right" % os.path.join(folder,"info.xml"))

    #If this is an mpeg2 myth recording and there is a cut list available and the user wants to use it
    #run mythtranscode to cut out commercials etc
    if file.attributes["type"].value == "recording":
        #can only use mythtranscode to cut commercials on mpeg2 files
        write("File codec is '%s'" % getVideoCodec(folder))
        if string.lower(getVideoCodec(folder)) == "mpeg2video": 
            if file.attributes["usecutlist"].value == "1" and getText(infoDOM.getElementsByTagName("hascutlist")[0]) == "yes":
                write("File has a cut list - attempting to run mythtrancode to remove unwanted segments")
                chanid = getText(infoDOM.getElementsByTagName("chanid")[0])
                starttime = getText(infoDOM.getElementsByTagName("starttime")[0])
                if runMythtranscode(chanid, starttime, os.path.join(folder,'tmp'), True):
                    mediafile = os.path.join(folder,'tmp')
                else:
                    write("Failed to run mythtranscode to remove unwanted segments")
            else:
                #does the user always want to run recordings through mythtranscode?
                #may help to fix any errors in the file 
                if alwaysRunMythtranscode == True:
                    write("Attempting to run mythtranscode --mpeg2 to fix any errors")
                    chanid = getText(infoDOM.getElementsByTagName("chanid")[0])
                    starttime = getText(infoDOM.getElementsByTagName("starttime")[0])
                    if runMythtranscode(chanid, starttime, os.path.join(folder, 'newfile.mpg'), False):
                        mediafile = os.path.join(folder, 'newfile.mpg')
                    else:
                        write("Failed to run mythtrancode to fix any errors")

    #do we need to re-encode the file to make it DVD compliant?
    if not isFileOkayForDVD(folder):
        #we need to re-encode the file, make sure we get the right video/audio streams
        #would be good if we could also split the file at the same time
        getStreamInformation(mediafile, os.path.join(folder, "streaminfo.xml"))

        #choose which streams we need
        video, audio1, audio2 = selectStreams(folder)

        #choose which aspect ratio we should use
        aspectratio = selectAspectRatio(folder)

        write("File is not DVD compliant - Re-encoding audio and video")

        #do the re-encode 
        encodeVideoToMPEG2(mediafile, os.path.join(folder, "newfile2.mpg"), video, audio1, audio2, aspectratio)
        mediafile = os.path.join(folder, 'newfile2.mpg')

    #remove an intermediate file
    if os.path.exists(os.path.join(folder, "newfile1.mpg")):
        os.remove(os.path.join(folder,'newfile1.mpg'))

    # the file is now DVD compliant split it into video and audio parts

    # find out what streams we have available now
    getStreamInformation(mediafile, os.path.join(folder, "streaminfo.xml"))

    # choose which streams we need
    video, audio1, audio2 = selectStreams(folder)

    # now attempt to split the source file into video and audio parts
    write("Splitting MPEG stream into audio and video parts")
    deMultiplexMPEG2File(folder, mediafile, video, audio1, audio2)

    if os.path.exists(os.path.join(folder, "newfile2.mpg")):
        os.remove(os.path.join(folder,'newfile2.mpg'))

    # we now have a video stream and one or more audio streams
    # check if we need to convert any of the audio streams to ac3
    processAudio(folder)

    #do a quick sense check before we continue...
    assert doesFileExist(os.path.join(folder,'stream.mv2'))
    assert doesFileExist(os.path.join(folder,'stream0.ac3'))
    #assert doesFileExist(os.path.join(folder,'stream1.ac3'))

    extractVideoFrame(os.path.join(folder,"stream.mv2"), os.path.join(folder,"thumbnail.jpg"), 0)

    write( "*************************************************************")
    write( "Finished processing file " + file.attributes["filename"].value)
    write( "*************************************************************")


def processJob(job):
    """Starts processing a MythBurn job, expects XML nodes to be passed as input."""
    global wantIntro, wantMainMenu, wantChapterMenu, wantDetailsPage
    global themeDOM, themeName, themeFonts

    media=job.getElementsByTagName("media")

    if media.length==1:

        themeName=job.attributes["theme"].value

        #Check theme exists
        if not validateTheme(themeName):
            fatalError("Failed to validate theme (%s)" % themeName)
        #Get the theme XML
        themeDOM = getThemeConfigurationXML(themeName)

        #Pre generate all the fonts we need
        loadFonts(themeDOM)

        #Update the global flags
        nodes=themeDOM.getElementsByTagName("intro")
        wantIntro = (nodes.length > 0)

        nodes=themeDOM.getElementsByTagName("menu")
        wantMainMenu = (nodes.length > 0)

        nodes=themeDOM.getElementsByTagName("submenu")
        wantChapterMenu = (nodes.length > 0)

        nodes=themeDOM.getElementsByTagName("detailspage")
        wantDetailsPage = (nodes.length > 0)

        write( "wantIntro: %d, wantMainMenu: %d, wantChapterMenu:%d, wantDetailsPage: %d" \
                % (wantIntro, wantMainMenu, wantChapterMenu, wantDetailsPage))

        if videomode=="ntsc":
            format=dvdNTSC
        elif videomode=="pal":
            format=dvdPAL
        else:
            fatalError("Unknown videomode is set (%s)" % videomode)

        write( "Final DVD Video format will be " + videomode)

        #Ensure the destination dvd folder is empty
        if doesFileExist(os.path.join(getTempPath(),"dvd")):
            deleteAllFilesInFolder(os.path.join(getTempPath(),"dvd"))

        #Loop through all the files
        files=media[0].getElementsByTagName("file")
        filecount=0
        if files.length > 0:
            write( "There are %s files to process" % files.length)

            if debug_secondrunthrough==False:
                #Delete all the temporary files that currently exist
                deleteAllFilesInFolder(getTempPath())

            #First pass through the files to be recorded - sense check
            #we dont want to find half way through this long process that
            #a file does not exist, or is the wrong format!!
            for node in files:
                filecount+=1

                #Generate a temp folder name for this file
                folder=getItemTempPath(filecount)

                if debug_secondrunthrough==False:
                    #If it already exists destroy it to remove previous debris
                    if os.path.exists(folder):
                        #Remove all the files first
                        deleteAllFilesInFolder(folder)
                        #Remove the folder
                        os.rmdir (folder)
                    os.makedirs(folder)
                #Do the pre-process work
                preProcessFile(node,folder)

            if debug_secondrunthrough==False:
                #Loop through all the files again but this time do more serious work!
                filecount=0
                for node in files:
                    filecount+=1
                    folder=getItemTempPath(filecount)

                    #Process this file
                    processFile(node,folder)

            #We can only create the menus after the videos have been processed
            #and the commercials cut out so we get the correct run time length
            #for the chapter marks and thumbnails.
            #create the DVD menus...
            if wantMainMenu:
                createMenu(format, files.length)

            #Submenus are visible when you select the chapter menu while the recording is playing
            if wantChapterMenu:
                createChapterMenu(format, files.length)

            #Details Page are displayed just before playing each recording
            if wantDetailsPage:
                createDetailsPage(format, files.length)

            #DVD Author file
            if not wantMainMenu and not wantChapterMenu:
                createDVDAuthorXMLNoMenus(format, files.length)
            elif not wantMainMenu:
                createDVDAuthorXMLNoMainMenu(format, files.length)
            else:
                createDVDAuthorXML(format, files.length)

            #Check all the files will fit onto a recordable DVD
            if mediatype == DVD_DL:
                # dual layer
                performMPEG2Shrink(files, dvdrsize[1])
            else:
                #single layer
                performMPEG2Shrink(files, dvdrsize[0])

            filecount=0
            for node in files:
                filecount+=1
                folder=getItemTempPath(filecount)
                #Multiplex this file
                #(This also removes non-required audio feeds inside mpeg streams 
                #(through re-multiplexing) we only take 1 video and 1 or 2 audio streams)
                pid=multiplexMPEGStream(os.path.join(folder,'stream.mv2'),
                        os.path.join(folder,'stream0.ac3'),
                        os.path.join(folder,'stream1.ac3'),
                        os.path.join(folder,'final.mpg'))

            #Now all the files are completed and ready to be burnt
            runDVDAuthor()

            #Create the DVD ISO image
            if docreateiso == True or mediatype == FILE:
                CreateDVDISO()

            #Burn the DVD ISO image
            if doburn == True and mediatype != FILE:
                BurnDVDISO()

            #Move the created iso image to the given location
            if mediatype == FILE and savefilename != "":
                write("Moving ISO image to: %s" % savefilename)
                os.rename(os.path.join(getTempPath(), 'mythburn.iso'), savefilename)

        else:
            write( "Nothing to do! (files)")
    else:
        write( "Nothing to do! (media)")
    return

def usage():
    write("""
    -h/--help               (Show this usage)
    -j/--jobfile file       (use file as the job file)
    -l/--progresslog file   (log file to output progress messages)

    """)

#
#
# The main starting point for mythburn.py
#
#

write( "mythburn.py (%s) starting up..." % VERSION)

nice=os.nice(8)
write( "Process priority %s" % nice)

#Ensure were running at least python 2.3.5
if not hasattr(sys, "hexversion") or sys.hexversion < 0x20305F0:
    sys.stderr.write("Sorry, your Python is too old. Please upgrade at least to 2.3.5\n")
    sys.exit(1)

# figure out where this script is located
scriptpath = os.path.dirname(sys.argv[0])
scriptpath = os.path.abspath(scriptpath)
write("script path:" + scriptpath)

# figure out where the myth share directory is located
sharepath = os.path.split(scriptpath)[0]
sharepath = os.path.split(sharepath)[0]
write("myth share path:" + sharepath)

# process any command line options
try:
    opts, args = getopt.getopt(sys.argv[1:], "j:hl:", ["jobfile=", "help", "progresslog="])
except getopt.GetoptError:
    # print usage and exit
    usage()
    sys.exit(2)

for o, a in opts:
    if o in ("-h", "--help"):
        usage()
        sys.exit()
    if o in ("-j", "--jobfile"):
        jobfile = str(a)
        write("passed job file: " + a)
    if o in ("-l", "--progresslog"):
        progresslog = str(a)
        write("passed progress log file: " + a)

#if we have been given a progresslog filename to write to open it
if progresslog != "":
    if os.path.exists(progresslog):
        os.remove(progresslog)
    progressfile = open(progresslog, 'w')
    write( "mythburn.py (%s) starting up..." % VERSION)

#if the script is run from the web interface the PATH environment variable does not include
#many of the bin locations we need so just append a few likely locations where our required
#executables may be
environPath = os.getenv("PATH")
os.environ['PATH'] += "/bin:/sbin:/usr/local/bin:/usr/bin:/opt/bin:"

#Get mysql database parameters
getMysqlDBParameters();

#Get defaults from MythTV database
defaultsettings = getDefaultParametersFromMythTVDB()
videopath = defaultsettings.get("VideoStartupDir", None)
recordingpath = defaultsettings.get("RecordFilePrefix", None)
gallerypath = defaultsettings.get("GalleryDir", None)
musicpath = defaultsettings.get("MusicLocation", None)
videomode = string.lower(defaultsettings["TVFormat"])
temppath = defaultsettings["MythArchiveTempDir"] + "/work"
logpath = defaultsettings["MythArchiveTempDir"] + "/logs"
dvddrivepath = defaultsettings["MythArchiveDVDLocation"]
dbVersion = defaultsettings["DBSchemaVer"]
preferredlang1 = defaultsettings["ISO639Language0"]
preferredlang2 = defaultsettings["ISO639Language1"]

# external commands
path_mplex = [defaultsettings["MythArchiveMplexCmd"], os.path.split(defaultsettings["MythArchiveMplexCmd"])[1]]
path_ffmpeg = [defaultsettings["MythArchiveFfmpegCmd"], os.path.split(defaultsettings["MythArchiveFfmpegCmd"])[1]]
path_dvdauthor = [defaultsettings["MythArchiveDvdauthorCmd"], os.path.split(defaultsettings["MythArchiveDvdauthorCmd"])[1]]
path_mkisofs = [defaultsettings["MythArchiveMkisofsCmd"], os.path.split(defaultsettings["MythArchiveMkisofsCmd"])[1]]
path_growisofs = [defaultsettings["MythArchiveGrowisofsCmd"], os.path.split(defaultsettings["MythArchiveGrowisofsCmd"])[1]]
path_tcrequant = [defaultsettings["MythArchiveTcrequantCmd"], os.path.split(defaultsettings["MythArchiveTcrequantCmd"])[1]]
path_png2yuv = [defaultsettings["MythArchivePng2yuvCmd"], os.path.split(defaultsettings["MythArchivePng2yuvCmd"])[1]]
path_spumux = [defaultsettings["MythArchiveSpumuxCmd"], os.path.split(defaultsettings["MythArchiveSpumuxCmd"])[1]]
path_mpeg2enc = [defaultsettings["MythArchiveMpeg2encCmd"], os.path.split(defaultsettings["MythArchiveMpeg2encCmd"])[1]]

try:
    try:
        # create our lock file so any UI knows we are running
        if os.path.exists(os.path.join(logpath, "mythburn.lck")):
            write("Lock File Exists - already running???")
            sys.exit(1)

        file = open(os.path.join(logpath, "mythburn.lck"), 'w')
        file.write("lock")
        file.close()

        #debug use 
        #videomode="ntsc"

        getTimeDateFormats()

        #Load XML input file from disk
        jobDOM = xml.dom.minidom.parse(jobfile)

        #Error out if its the wrong XML
        if jobDOM.documentElement.tagName != "mythburn":
            fatalError("Job file doesn't look right!")

        #process each job
        jobcount=0
        jobs=jobDOM.getElementsByTagName("job")
        for job in jobs:
            jobcount+=1
            write( "Processing Mythburn job number %s." % jobcount)

            #get any options from the job file if present
            options = job.getElementsByTagName("options")
            if options.length > 0:
                getOptions(options)

            processJob(job)

        jobDOM.unlink()

        write("Finished processing jobs!!!")
    finally:
        # remove our lock file
        if os.path.exists(os.path.join(logpath, "mythburn.lck")):
            os.remove(os.path.join(logpath, "mythburn.lck"))

        # make sure the files we created are read/writable by all 
        os.system("chmod -R a+rw-x+X %s" % defaultsettings["MythArchiveTempDir"]) 
except SystemExit:
    write("Terminated")
except:
    write('-'*60)
    traceback.print_exc(file=sys.stdout)
    if progresslog != "":
        traceback.print_exc(file=progressfile)
    write('-'*60)