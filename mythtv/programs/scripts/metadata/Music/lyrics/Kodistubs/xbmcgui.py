# Tiny portion of Kodistubs to translate CU LRC to MythTV.  If CU LRC
# ever refers to other functions, simply copy them in here from
# original Kodistubs and update where needed like below.

# This file is generated from Kodi source code and post-edited
# to correct code style and docstrings formatting.
# License: GPL v.3 <https://www.gnu.org/licenses/gpl-3.0.en.html>
"""
**GUI functions on Kodi.**

Offers classes and functions that manipulate the Graphical User Interface
through windows, dialogs, and various control widgets.
"""
#from typing import Union, List, Dict, Tuple, Optional

# for MythtTV, show "dialog" on stderr
import sys

class Dialog:
    """
    **Kodi's dialog class**
    """
    
    def __init__(self) -> None:
        pass
    
    def ok(self, heading: str, message: str) -> bool:
        """
        **OK dialog**
        """
        return True

class DialogProgress:
    """
    **Kodi's progress dialog class (Duh!)**
    """
    
    def __init__(self) -> None:
        pass
    
    def create(self, heading: str, message: str = "") -> None:
        """
        Create and show a progress dialog.
        """
        print("\tDIALOG created: ", heading, " : ", message, file=sys.stderr)
        pass
    
    def update(self, percent: int, message: str = "") -> None:
        """
        Updates the progress dialog.
        """
        print("\tDIALOG updated %s: " % percent, message, file=sys.stderr)
        pass
    
    def close(self) -> None:
        """
        Close the progress dialog.
        """
        pass
    
    def iscanceled(self) -> bool:
        """
        Checks progress is canceled.
        """
        # for MythTV, not cancelled is needed to continue the scrapertest.py
        return False

class Window:
    """
    **GUI window class for Add-Ons.**
    """
    
    def __init__(self, existingWindowId: int = -1) -> None:
        pass
