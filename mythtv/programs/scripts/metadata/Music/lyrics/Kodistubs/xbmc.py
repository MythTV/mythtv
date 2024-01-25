# Tiny portion of Kodistubs to translate CU LRC to MythTV.  If CU LRC
# ever refers to other functions, simply copy them in here from
# original Kodistubs and update where needed like below.

import sys

LOGDEBUG = 0

def log(msg: str, level: int = LOGDEBUG) -> None:
    print(msg, file=sys.stderr)
