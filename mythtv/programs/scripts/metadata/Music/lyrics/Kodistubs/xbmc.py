# Tiny portion of Kodistubs to translate CU LRC to MythTV.  If CU LRC
# ever refers to other functions, simply copy them in here from
# original Kodistubs and update where needed like below.

# This file is generated from Kodi source code and post-edited
# to correct code style and docstrings formatting.
# License: GPL v.3 <https://www.gnu.org/licenses/gpl-3.0.en.html>
"""
**General functions on Kodi.**

Offers classes and functions that provide information about the media currently
playing and that allow manipulation of the media player (such as starting a new
song). You can also find system information using the functions available in
this library.
"""

LOGDEBUG = 0

def log(msg: str, level: int = LOGDEBUG) -> None:
    """
    Write a string to Kodi's log file and the debug window.
    """
    # for MythTV, simply send debugging messages to stderr
    import sys
    print("debug=%s %s" % (level, msg), file=sys.stderr)
    pass
