# Tiny portion of Kodistubs to translate CU LRC to MythTV.  If CU LRC
# ever refers to other functions, simply copy them in here from
# original Kodistubs and update where needed like below.

# This file is generated from Kodi source code and post-edited
# to correct code style and docstrings formatting.
# License: GPL v.3 <https://www.gnu.org/licenses/gpl-3.0.en.html>
"""
**Virtual file system functions on Kodi.**

Offers classes and functions offers access to the Virtual File Server (VFS)
which you can use to manipulate files and folders.
"""
from typing import Union, List, Tuple, Optional

# for MythTV, embedlrc.py and musixmatchlrc.py need some File access

class File:
    """
    **Kodi's file class.**
    """
    
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
        """
        Read file parts as string.
        """
        if numBytes:
            return self.fh.read(numBytes)
        return self.fh.read()
    
    def readBytes(self, numBytes: int = 0) -> bytearray:
        """
        Read bytes from file.
        """
        if numBytes:
            return bytearray(self.fh.read(numBytes))
        return bytearray(self.fh.read())
    
    def write(self, buffer: Union[str,  bytes,  bytearray]) -> bool:
        """
        To write given data in file.
        """
        return True
    
    def size(self) -> int:
        """
        Get the file size.
        """
        return 0
    
    def seek(self, seekBytes: int, iWhence: int = 0) -> int:
        """
        Seek to position in file.
        """
        return self.fh.seek(seekBytes, iWhence);
    
    def tell(self) -> int:
        """
        Get the current position in the file.
        """
        return self.fh.tell()
    
    def close(self) -> None:
        """
        Close opened file.
        """
        self.fh.close()
        pass
    
def exists(path: str) -> bool:
    """
    Check for a file or folder existence
    """
    # for musixmatchlrc.py the test must work or return False
    return False

def translatePath(path: str) -> str:
    """
    Returns the translated path.
    """
    return ""
