# This file is generated from Kodi source code and post-edited
# to correct code style and docstrings formatting.
# License: GPL v.3 <https://www.gnu.org/licenses/gpl-3.0.en.html>
"""
**Kodi's addon class.**
"""
from typing import List, Optional

__kodistubs__ = True




class Addon:
    """
    **Kodi's addon class.**

    Offers classes and functions that manipulate the add-on settings, information
    and localization.

    Creates a new AddOn class.

    :param id: [opt] string - id of the addon as specified inaddon.xml

    .. note::
        Specifying the addon id is not needed.  Important however is that
        the addon folder has the same name as the AddOn id provided
        inaddon.xml.  You can optionally specify the addon id from another
        installed addon to retrieve settings from it.

    @python_v13 **id** is optional as it will be auto detected for this
    add-on instance.

    Example::

        ..
        self.Addon = xbmcaddon.Addon()
        self.Addon = xbmcaddon.Addon('script.foo.bar')
        ..
    """
    
    def __init__(self, id: Optional[str] = None) -> None:
        pass
    
    def getLocalizedString(self, id: int) -> str:
        """
        Returns an addon's localized 'string'.

        :param id: integer - id# for string you want to localize.
        :return: Localized 'string'

        @python_v13 **id** is optional as it will be auto detected for this
        add-on instance.

        Example::

            ..
            locstr = self.Addon.`getLocalizedString`(32000)
            ..
        """
        return ""
    
    def getSettings(self) -> 'Settings':
        """
        Returns a wrapper around the addon's settings.

        :return: `Settings` wrapper

        @python_v20 New function added.

        Example::

            ..
            settings = self.Addon.getSettings()
            ..
        """
        return Settings()
    
    def getSetting(self, id: str) -> str:
        """
        Returns the value of a setting as string.

        :param id: string - id of the setting that the module needs to access.
        :return: Setting as a string

        @python_v13 **id** is optional as it will be auto detected for this
        add-on instance.

        Example::

            ..
            apikey = self.Addon.getSetting('apikey')
            ..
        """
        return ""
    
    def getSettingBool(self, id: str) -> bool:
        """
        Returns the value of a setting as a boolean.

        :param id: string - id of the setting that the module needs to access.
        :return: Setting as a boolean

        @python_v18 New function added.

        @python_v20 Deprecated. Use **`Settings.getBool()`** instead.

        Example::

            ..
            enabled = self.Addon.getSettingBool('enabled')
            ..
        """
        return True
    
    def getSettingInt(self, id: str) -> int:
        """
        Returns the value of a setting as an integer.

        :param id: string - id of the setting that the module needs to access.
        :return: Setting as an integer

        @python_v18 New function added.

        @python_v20 Deprecated. Use **`Settings.getInt()`** instead.

        Example::

            ..
            max = self.Addon.getSettingInt('max')
            ..
        """
        return 0
    
    def getSettingNumber(self, id: str) -> float:
        """
        Returns the value of a setting as a floating point number.

        :param id: string - id of the setting that the module needs to access.
        :return: Setting as a floating point number

        @python_v18 New function added.

        @python_v20 Deprecated. Use **`Settings.getNumber()`** instead.

        Example::

            ..
            max = self.Addon.getSettingNumber('max')
            ..
        """
        return 0.0
    
    def getSettingString(self, id: str) -> str:
        """
        Returns the value of a setting as a string.

        :param id: string - id of the setting that the module needs to access.
        :return: Setting as a string

        @python_v18 New function added.

        @python_v20 Deprecated. Use **`Settings.getString()`** instead.

        Example::

            ..
            apikey = self.Addon.getSettingString('apikey')
            ..
        """
        return ""
    
    def setSetting(self, id: str, value: str) -> None:
        """
        Sets a script setting.

        :param id: string - id of the setting that the module needs to access.
        :param value: string - value of the setting.

        .. note::
            You can use the above as keywords for arguments.

        @python_v13 **id** is optional as it will be auto detected for this
        add-on instance.

        Example::

            ..
            self.Addon.`setSetting`(id='username', value='teamkodi')
            ..
        """
        pass
    
    def setSettingBool(self, id: str, value: bool) -> bool:
        """
        Sets a script setting.

        :param id: string - id of the setting that the module needs to access.
        :param value: boolean - value of the setting.
        :return: True if the value of the setting was set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v18 New function added.

        @python_v20 Deprecated. Use **`Settings.setBool()`** instead.

        Example::

            ..
            self.Addon.setSettingBool(id='enabled', value=True)
            ..
        """
        return True
    
    def setSettingInt(self, id: str, value: int) -> bool:
        """
        Sets a script setting.

        :param id: string - id of the setting that the module needs to access.
        :param value: integer - value of the setting.
        :return: True if the value of the setting was set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v18 New function added.

        @python_v20 Deprecated. Use **`Settings.setInt()`** instead.

        Example::

            ..
            self.Addon.setSettingInt(id='max', value=5)
            ..
        """
        return True
    
    def setSettingNumber(self, id: str, value: float) -> bool:
        """
        Sets a script setting.

        :param id: string - id of the setting that the module needs to access.
        :param value: float - value of the setting.
        :return: True if the value of the setting was set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v18 New function added.

        @python_v20 Deprecated. Use **`Settings.setNumber()`** instead.

        Example::

            ..
            self.Addon.setSettingNumber(id='max', value=5.5)
            ..
        """
        return True
    
    def setSettingString(self, id: str, value: str) -> bool:
        """
        Sets a script setting.

        :param id: string - id of the setting that the module needs to access.
        :param value: string or unicode - value of the setting.
        :return: True if the value of the setting was set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v18 New function added.

        @python_v20 Deprecated. Use **`Settings.setString()`** instead.

        Example::

            ..
            self.Addon.setSettingString(id='username', value='teamkodi')
            ..
        """
        return True
    
    def openSettings(self) -> None:
        """
        Opens this scripts settings dialog.

        Example::

            ..
            self.Addon.openSettings()
            ..
        """
        pass
    
    def getAddonInfo(self, id: str) -> str:
        """
        Returns the value of an addon property as a string.

        :param id: string - id of the property that the module needs to access.

        Choices for the property are

        ====== ========= =========== ========== 
        author changelog description disclaimer 
        fanart icon      id          name       
        path   profile   stars       summary    
        type   version                          
        ====== ========= =========== ========== 

        :return: AddOn property as a string

        Example::

            ..
            version = self.Addon.getAddonInfo('version')
            ..
        """
        return ""
    

class Settings:
    """
    **Add-on settings**

    This wrapper provides access to the settings specific to an add-on. It supports
    reading and writing specific setting values.

    @python_v20 New class added.

    Example::

        ...
        settings = xbmcaddon.Addon('id').getSettings()
        ...
    """
    
    def getBool(self, id: str) -> bool:
        """
        Returns the value of a setting as a boolean.

        :param id: string - id of the setting that the module needs to access.
        :return: bool - Setting as a boolean

        @python_v20 New function added.

        Example::

            ..
            enabled = settings.getBool('enabled')
            ..
        """
        return True
    
    def getInt(self, id: str) -> int:
        """
        Returns the value of a setting as an integer.

        :param id: string - id of the setting that the module needs to access.
        :return: integer - Setting as an integer

        @python_v20 New function added.

        Example::

            ..
            max = settings.getInt('max')
            ..
        """
        return 0
    
    def getNumber(self, id: str) -> float:
        """
        Returns the value of a setting as a floating point number.

        :param id: string - id of the setting that the module needs to access.
        :return: float - Setting as a floating point number

        @python_v20 New function added.

        Example::

            ..
            max = settings.getNumber('max')
            ..
        """
        return 0.0
    
    def getString(self, id: str) -> str:
        """
        Returns the value of a setting as a unicode string.

        :param id: string - id of the setting that the module needs to access.
        :return: string - Setting as a unicode string

        @python_v20 New function added.

        Example::

            ..
            apikey = settings.getString('apikey')
            ..
        """
        return ""
    
    def getBoolList(self, id: str) -> List[bool]:
        """
        Returns the value of a setting as a list of booleans.

        :param id: string - id of the setting that the module needs to access.
        :return: list - Setting as a list of booleans

        @python_v20 New function added.

        Example::

            ..
            enabled = settings.getBoolList('enabled')
            ..
        """
        return [True]
    
    def getIntList(self, id: str) -> List[int]:
        """
        Returns the value of a setting as a list of integers.

        :param id: string - id of the setting that the module needs to access.
        :return: list - Setting as a list of integers

        @python_v20 New function added.

        Example::

            ..
            ids = settings.getIntList('ids')
            ..
        """
        return [0]
    
    def getNumberList(self, id: str) -> List[float]:
        """
        Returns the value of a setting as a list of floating point numbers.

        :param id: string - id of the setting that the module needs to access.
        :return: list - Setting as a list of floating point numbers

        @python_v20 New function added.

        Example::

            ..
            max = settings.getNumberList('max')
            ..
        """
        return [0.0]
    
    def getStringList(self, id: str) -> List[str]:
        """
        Returns the value of a setting as a list of unicode strings.

        :param id: string - id of the setting that the module needs to access.
        :return: list - Setting as a list of unicode strings

        @python_v20 New function added.

        Example::

            ..
            views = settings.getStringList('views')
            ..
        """
        return [""]
    
    def setBool(self, id: str, value: bool) -> None:
        """
        Sets the value of a setting.

        :param id: string - id of the setting that the module needs to access.
        :param value: bool - value of the setting.
        :return: bool - True if the value of the setting was set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v20 New function added.

        Example::

            ..
            settings.setBool(id='enabled', value=True)
            ..
        """
        pass
    
    def setInt(self, id: str, value: int) -> None:
        """
        Sets the value of a setting.

        :param id: string - id of the setting that the module needs to access.
        :param value: integer - value of the setting.
        :return: bool - True if the value of the setting was set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v20 New function added.

        Example::

            ..
            settings.setInt(id='max', value=5)
            ..
        """
        pass
    
    def setNumber(self, id: str, value: float) -> None:
        """
        Sets the value of a setting.

        :param id: string - id of the setting that the module needs to access.
        :param value: float - value of the setting.
        :return: bool - True if the value of the setting was set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v20 New function added.

        Example::

            ..
            settings.setNumber(id='max', value=5.5)
            ..
        """
        pass
    
    def setString(self, id: str, value: str) -> None:
        """
        Sets the value of a setting.

        :param id: string - id of the setting that the module needs to access.
        :param value: string or unicode - value of the setting.
        :return: bool - True if the value of the setting was set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v20 New function added.

        Example::

            ..
            settings.setString(id='username', value='teamkodi')
            ..
        """
        pass
    
    def setBoolList(self, id: str, values: List[bool]) -> None:
        """
        Sets the boolean values of a list setting.

        :param id: string - id of the setting that the module needs to access.
        :param values: list of boolean - values of the setting.
        :return: bool - True if the values of the setting were set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v20 New function added.

        Example::

            ..
            settings.setBoolList(id='enabled', values=[ True, False ])
            ..
        """
        pass
    
    def setIntList(self, id: str, values: List[int]) -> None:
        """
        Sets the integer values of a list setting.

        :param id: string - id of the setting that the module needs to access.
        :param values: list of int - values of the setting.
        :return: bool - True if the values of the setting were set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v20 New function added.

        Example::

            ..
            settings.setIntList(id='max', values=[ 5, 23 ])
            ..
        """
        pass
    
    def setNumberList(self, id: str, values: List[float]) -> None:
        """
        Sets the floating point values of a list setting.

        :param id: string - id of the setting that the module needs to access.
        :param values: list of float - values of the setting.
        :return: bool - True if the values of the setting were set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v20 New function added.

        Example::

            ..
            settings.setNumberList(id='max', values=[ 5.5, 5.8 ])
            ..
        """
        pass
    
    def setStringList(self, id: str, values: List[str]) -> None:
        """
        Sets the string values of a list setting.

        :param id: string - id of the setting that the module needs to access.
        :param values: list of string or unicode - values of the setting.
        :return: bool - True if the values of the setting were set, false otherwise

        .. note::
            You can use the above as keywords for arguments.

        @python_v20 New function added.

        Example::

            ..
            settings.setStringList(id='username', values=[ 'team', 'kodi' ])
            ..
        """
        pass
