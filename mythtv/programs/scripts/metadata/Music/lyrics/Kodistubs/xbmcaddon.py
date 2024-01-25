# Tiny portion of Kodistubs to translate CU LRC to MythTV.  If CU LRC
# ever refers to other functions, simply copy them in here from
# original Kodistubs and update where needed like below.

from typing import Optional

class Addon:

    def __init__(self, id: Optional[str] = None) -> None:
        pass

    def getLocalizedString(self, id: int) -> str:
        # only testall.py / scrapertest.py needs only 1 message from
        # resources/language/resource.language.en_us/strings.po
        if (id == 32163):
            return "Testing: %s"
        return "(%s)" % id

    def getSettingBool(self, id: str) -> bool:
        return True

    def getSettingInt(self, id: str) -> int:
        return 0

    def getSettingString(self, id: str) -> str:
        return ""

    def getAddonInfo(self, id: str) -> str:
        return ""
