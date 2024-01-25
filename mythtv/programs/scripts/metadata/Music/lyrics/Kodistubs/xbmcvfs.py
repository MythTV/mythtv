# Tiny portion of Kodistubs to translate CU LRC to MythTV.  If CU LRC
# ever refers to other functions, simply copy them in here from
# original Kodistubs and update where needed like below.

from typing import Union, Optional
import os.path

# embedlrc.py and musixmatchlrc.py need some File access

class File:

    def __init__(self, filepath: str, mode: Optional[str] = None) -> None:
        self.filename = filepath
        if mode:
            self.fh = open(filepath, mode)
        else:
            self.fh = open(filepath, "rb")

    def __enter__(self) -> 'File':  # Required for context manager
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):  # Required for context manager
        pass

    def read(self, numBytes: int = 0) -> str:
        if numBytes:
            return self.fh.read(numBytes)
        return self.fh.read()

    def readBytes(self, numBytes: int = 0) -> bytearray:
        if numBytes:
            return bytearray(self.fh.read(numBytes))
        return bytearray(self.fh.read())

    def write(self, buffer: Union[str,  bytes,  bytearray]) -> bool:
        return self.fh.write(buffer)

    def size(self) -> int:
        return 0

    def seek(self, seekBytes: int, iWhence: int = 0) -> int:
        return self.fh.seek(seekBytes, iWhence);

    def tell(self) -> int:
        return self.fh.tell()

    def close(self) -> None:
        self.fh.close()
        pass

def exists(path: str) -> bool:
    # for musixmatchlrc.py the test must work or return False
    return os.path.isfile(path)

def translatePath(path: str) -> str:
    return ""
