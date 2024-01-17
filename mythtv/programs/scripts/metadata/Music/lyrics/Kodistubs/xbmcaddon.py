# Tiny portion of Kodistubs to translate CU LRC to MythTV.  If CU LRC
# ever refers to other functions, simply copy them in here from
# original Kodistubs and update where needed like below.

# This file is generated from Kodi source code and post-edited
# to correct code style and docstrings formatting.
# License: GPL v.3 <https://www.gnu.org/licenses/gpl-3.0.en.html>
"""
**Kodi's addon class.**
"""
from typing import List, Optional

class Addon:
    """
    **Kodi's addon class.**
    """
    
    def __init__(self, id: Optional[str] = None) -> None:
        pass
    
    def getLocalizedString(self, id: int) -> str:
        """
        Returns an addon's localized 'string'.
        """
        # for MythTV, testall/scrapertest needs only 1 message from
        # resources/language/resource.language.en_us/strings.po
        if (id == 32163):
            return "Testing: %s"
        return "(%s)" % id
    
    def getSettingBool(self, id: str) -> bool:
        """
        Returns the value of a setting as a boolean.
        """
        return True
    
    def getSettingInt(self, id: str) -> int:
        """
        Returns the value of a setting as an integer.
        """
        return 0
    
    def getSettingString(self, id: str) -> str:
        """
        Returns the value of a setting as a string.
        """
        return ""

    def getAddonInfo(self, id: str) -> str:
        """
        Returns the value of an addon property as a string.
        """
        return ""
