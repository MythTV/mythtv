# This file is generated from Kodi source code and post-edited
# to correct code style and docstrings formatting.
# License: GPL v.3 <https://www.gnu.org/licenses/gpl-3.0.en.html>
"""
**Virtual file system functions on Kodi.**

Offers classes and functions offers access to the Virtual File Server (VFS)
which you can use to manipulate files and folders.
"""
from typing import Union, List, Tuple, Optional

__kodistubs__ = True


class File:
    """
    **Kodi's file class.**

    :param filepath: string Selected file path
    :param mode: [opt] string Additional mode options (if no mode is supplied, the
        default is Open for Read).

    ==== ============== 
    Mode Description    
    ==== ============== 
    w    Open for write 
    ==== ============== 

    @python_v19 Added context manager support

    Example::

        ..
        f = xbmcvfs.File(file, 'w')
        ..

    Example (v19 and up)::

        ..
        with xbmcvfs.File(file, 'w') as f:
            ..
            ..
    """
    
    def __init__(self, filepath: str, mode: Optional[str] = None) -> None:
        pass

    def __enter__(self) -> 'File':  # Required for context manager
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):  # Required for context manager
        pass
    
    def read(self, numBytes: int = 0) -> str:
        """
        Read file parts as string.

        :param bytes: [opt] How many bytes to read - if not set it will read the whole file
        :return: string

        Example::

            ..
            f = xbmcvfs.File(file)
            b = f.read()
            f.close()
            ..

        Example (v19 and up)::

            ..
            with xbmcvfs.File(file) as file:
                b = f.read()
                ..
        """
        return ""
    
    def readBytes(self, numBytes: int = 0) -> bytearray:
        """
        Read bytes from file.

        :param numbytes: How many bytes to read [opt]- if not set it will read the whole file
        :return: bytearray

        Example::

            ..
            f = xbmcvfs.File(file)
            b = f.readBytes()
            f.close()
            ..

        Example (v19 and up)::

            ..
            with xbmcvfs.File(file) as f:
                b = f.readBytes()
                ..
        """
        return bytearray()
    
    def write(self, buffer: Union[str,  bytes,  bytearray]) -> bool:
        """
        To write given data in file.

        :param buffer: Buffer to write to file
        :return: True on success.

        Example::

            ..
            f = xbmcvfs.File(file, 'w')
            result = f.write(buffer)
            f.close()
            ..

        Example (v19 and up)::

            ..
            with xbmcvfs.File(file, 'w') as f:
                result = f.write(buffer)
                ..
        """
        return True
    
    def size(self) -> int:
        """
        Get the file size.

        :return: The file size

        Example::

            ..
            f = xbmcvfs.File(file)
            s = f.size()
            f.close()
            ..

        Example (v19 and up)::

            ..
            with xbmcvfs.File(file) as f:
                s = f.size()
                ..
        """
        return 0
    
    def seek(self, seekBytes: int, iWhence: int = 0) -> int:
        """
        Seek to position in file.

        :param seekBytes: position in the file
        :param iWhence: [opt] where in a file to seek from[0 beginning, 1 current , 2 end
            position]

        @python_v19 Function changed. param **iWhence** is now optional.

        Example::

            ..
            f = xbmcvfs.File(file)
            result = f.seek(8129, 0)
            f.close()
            ..

        Example (v19 and up)::

            ..
            with xbmcvfs.File(file) as f:
                result = f.seek(8129, 0)
                ..
        """
        return 0
    
    def tell(self) -> int:
        """
        Get the current position in the file.

        :return: The file position

        @python_v19 New function added

        Example::

            ..
            f = xbmcvfs.File(file)
            s = f.tell()
            f.close()
            ..

        Example (v19 and up)::

            ..
            with xbmcvfs.File(file) as f:
                s = f.tell()
                ..
        """
        return 0
    
    def close(self) -> None:
        """
        Close opened file.

        Example::

            ..
            f = xbmcvfs.File(file)
            f.close()
            ..

        Example (v19 and up)::

            ..
            with xbmcvfs.File(file) as f:
                ..
                ..
        """
        pass
    

class Stat:
    """
    **Get file or file system status.**

    These class return information about a file. Execute (search) permission is
    required on all of the directories in path that lead to the file.

    :param path: [string] file or folder

    @python_v12 New function added

    Example::

        ..
        st = xbmcvfs.Stat(path)
        modified = st.st_mtime()
        ..
    """
    
    def __init__(self, path: str) -> None:
        pass
    
    def st_mode(self) -> int:
        """
        To get file protection.

        :return: st_mode
        """
        return 0
    
    def st_ino(self) -> int:
        """
        To get inode number.

        :return: st_ino
        """
        return 0
    
    def st_dev(self) -> int:
        """
        To get ID of device containing file.

        The st_dev field describes the device on which this file resides.

        :return: st_dev
        """
        return 0
    
    def st_nlink(self) -> int:
        """
        To get number of hard links.

        :return: st_nlink
        """
        return 0
    
    def st_uid(self) -> int:
        """
        To get user ID of owner.

        :return: st_uid
        """
        return 0
    
    def st_gid(self) -> int:
        """
        To get group ID of owner.

        :return: st_gid
        """
        return 0
    
    def st_size(self) -> int:
        """
        To get total size, in bytes.

        The st_size field gives the size of the file (if it is a regular file or a
        symbolic link) in bytes. The size of a symbolic link (only on Linux and Mac OS X)
        is the length of the pathname it contains, without a terminating null byte.

        :return: st_size
        """
        return 0
    
    def st_atime(self) -> int:
        """
        To get time of last access.

        :return: st_atime
        """
        return 0
    
    def st_mtime(self) -> int:
        """
        To get time of last modification.

        :return: st_mtime
        """
        return 0
    
    def st_ctime(self) -> int:
        """
        To get time of last status change.

        :return: st_ctime
        """
        return 0
    

def copy(strSource: str, strDestination: str) -> bool:
    """
    Copy file to destination, returns true/false.

    :param source: file to copy.
    :param destination: destination file
    :return: True if successed

    Example::

        ..
        success = xbmcvfs.copy(source, destination)
        ..
    """
    return True


def delete(file: str) -> bool:
    """
    Delete a file

    :param file: File to delete
    :return: True if successed

    Example::

        ..
        xbmcvfs.delete(file)
        ..
    """
    return True


def rename(file: str, newFile: str) -> bool:
    """
    Rename a file

    :param file: File to rename
    :param newFileName: New filename, including the full path
    :return: True if successed

    .. note::
        Moving files between different filesystem (eg. local to nfs://) is
        not possible on all platforms. You may have to do it manually by
        using the copy and deleteFile functions.

    Example::

        ..
        success = xbmcvfs.rename(file,newFileName)
        ..
    """
    return True


def exists(path: str) -> bool:
    """
    Check for a file or folder existence

    :param path: File or folder (folder must end with slash or backslash)
    :return: True if successed

    Example::

        ..
        success = xbmcvfs.exists(path)
        ..
    """
    return True


def makeLegalFilename(filename: str) -> str:
    """
    Returns a legal filename or path as a string.

    :param filename: string - filename/path to make legal
    :return: Legal filename or path as a string

    .. note::
        The returned value is platform-specific. This is due to the fact
        that the chars that need to be replaced to make a path legal
        depend on the underlying OS filesystem. This is useful, for
        example, if you want to create a file or folder based on data over
        which you have no control (e.g. an external API).

    @python_v19 New function added (replaces old **xbmc.makeLegalFilename**)

    Example::

        ..
        # windows
        >> xbmcvfs.makeLegalFilename('C://Trailers/Ice Age: The Meltdown.avi')
        C:\Trailers\Ice Age_ The Meltdown.avi
        # non-windows
        >> xbmcvfs.makeLegalFilename("///\\jk???lj????.mpg")
        /jk___lj____.mpg
        ..
    """
    return ""


def translatePath(path: str) -> str:
    """
    Returns the translated path.

    :param path: string - Path to format
    :return: Translated path

    .. note::
        Only useful if you are coding for both Linux and Windows. e.g.
        Converts 'special://home' -> '/home/[username]/.kodi' on Linux.

    @python_v19 New function added (replaces old **xbmc.translatePath**)

    Example::

        ..
        fpath = xbmcvfs.translatePath('special://home')
        ..
    """
    return ""


def validatePath(path: str) -> str:
    """
    Returns the validated path.

    :param path: string - Path to format
    :return: Validated path

    .. note::
        The result is platform-specific. Only useful if you are coding for
        multiple platfforms for fixing slash problems (e.g. Corrects
        'Z://something' -> 'Z:\something').

    @python_v19 New function added (replaces old **xbmc.validatePath**)

    Example::

        ..
        fpath = xbmcvfs.validatePath(somepath)
        ..
    """
    return ""


def mkdir(path: str) -> bool:
    """
    Create a folder.

    :param path: Folder to create
    :return: True if successed

    Example::

        ..
        success = xbmcvfs.mkdir(path)
        ..
    """
    return True


def mkdirs(path: str) -> bool:
    """
    Make all directories along the path

    Create folder(s) - it will create all folders in the path.

    :param path: Folders to create
    :return: True if successed

    Example::

        ..
        success = xbmcvfs.mkdirs(path)
        ..
    """
    return True


def rmdir(path: str, force: bool = False) -> bool:
    """
    Remove a folder.

    :param path: string - Folder to remove
    :param force: [opt] bool - Force directory removal (default False). This can be
        useful if the directory is not empty.
    :return: bool - True if successful, False otherwise

    Example::

        ..
        success = xbmcvfs.rmdir(path)
        ..
    """
    return True


def listdir(path: str) -> Tuple[List[str], List[str]]:
    """
    Lists content of a folder.

    :param path: Folder to get list from
    :return: Directory content list

    Example::

        ..
        dirs, files = xbmcvfs.listdir(path)
        ..
    """
    return [""], [""]
