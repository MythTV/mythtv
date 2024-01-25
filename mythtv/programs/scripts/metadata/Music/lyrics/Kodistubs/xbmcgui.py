# Tiny portion of Kodistubs to translate CU LRC to MythTV.  If CU LRC
# ever refers to other functions, simply copy them in here from
# original Kodistubs and update where needed like below.

# show "dialog" on stderr for testall.py / scrapertest.py
import sys

class Dialog:

    def __init__(self) -> None:
        pass

    def ok(self, heading: str, message: str) -> bool:
        return True

class DialogProgress:

    def __init__(self) -> None:
        pass

    def create(self, heading: str, message: str = "") -> None:
        print("\tDIALOG created: ", heading, " : ", message, file=sys.stderr)
        pass

    def update(self, percent: int, message: str = "") -> None:
        print("\tDIALOG updated %s: " % percent, message, file=sys.stderr)
        pass

    def close(self) -> None:
        pass

    def iscanceled(self) -> bool:
        # not cancelled is needed to continue the testall.py / scrapertest.py
        return False

class Window:
    def __init__(self, existingWindowId: int = -1) -> None:
        pass
