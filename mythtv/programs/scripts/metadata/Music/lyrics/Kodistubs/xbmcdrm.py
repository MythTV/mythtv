# This file is generated from Kodi source code and post-edited
# to correct code style and docstrings formatting.
# License: GPL v.3 <https://www.gnu.org/licenses/gpl-3.0.en.html>
"""
**Kodi's DRM class.**
"""
from typing import Union, Dict

__kodistubs__ = True


class CryptoSession:
    """
    **Kodi's DRM class.**

    Offers classes and functions that allow a developer to work with DRM-protected
    contents like Widevine.

    This type of functionality is closely related to the type of DRM used and the
    service to be implemented.

    Using the `CryptoSession` constructor allow you to have access to a DRM session.
    With a DRM session you can read and write the DRM
    properties `GetPropertyString`,`SetPropertyString` and establish session keys
    with GetKeyRequest and `ProvideKeyResponse`, or resume previous session keys
    with `RestoreKeys`.

    When the session keys are established you can use these methods to perform
    various operations:`Encrypt` /`Decrypt` for data encryption / decryption,`Sign`
    /`Verify` for make or verify data-signature. Useful for example to implement
    encrypted communication between a client and the server.

    An example where such functionality is useful is the Message Security Layer
    (MSL) transmission protocol used in some VOD applications. This protocol (or
    rather framework) is used to increase the level of security in the exchange of
    messages (such as licences, manifests or other data), which defines a security
    extension / layer on top of the HTTP protocol.

    Constructor for DRM crypto session

    :param UUID: string - 16 byte UUID of the DRM system to use
    :param cipherAlgorithm: string - Algorithm used for encryption / decryption ciphers
    :param macAlgorithm: string - Algorithm used for sign / verify
    :raises RuntimeException: If the session can not be established

    @python_v18 New class added.

    Example::

        ..
        uuid_widevine = 'edef8ba9-79d6-4ace-a3c8-27dcd51d21ed'
        crypto_session = xbmcdrm.CryptoSession(uuid_widevine, 'AES/CBC/NoPadding', 'HmacSHA256')
        ..
    """
    
    def __init__(self, UUID: str,
                 cipherAlgorithm: str,
                 macAlgorithm: str) -> None:
        pass
    
    def GetKeyRequest(self, init: Union[str, bytes, bytearray],
                      mimeType: str,
                      offlineKey: bool,
                      optionalParameters: Dict[str, str]) -> bytearray:
        """
        Generate a key request

        Generate a key request, used for request/response exchange between the app and a
        license server to obtain or release keys used to decrypt encrypted content.
        After the app has received the key request response from the license server, it
        should deliver to the response to the DRM instance using the
        method `ProvideKeyResponse`, to activate the keys.

        :param init: byte - Initialization bytes container-specific data, its meaning is
            interpreted based on the mime type provided in the mimeType
            parameter. It could contain, for example, the content ID, key ID
            or other data required in generating the key request.
        :param mimeType: string - Type of media which is exchanged (e.g. "application/xml",
            "video/mp4")
        :param offlineKey: bool - Specifies the type of the request. The request may be to
            acquire keys for Streaming or Offline content
        :param optionalParameters: [opt] map - Will be included in the key request message to allow a
            client application to provide additional message parameters to the
            server
        :return: byte - The opaque key request data (challenge) which is send to key server

        @python_v18 New function added.

        @python_v19 With python 3 the init param must be a bytearray instead of byte.
        """
        return bytearray()
    
    def GetPropertyString(self, name: str) -> str:
        """
        Request a system specific property value of the DRM system.

        :param Name: string - Name of the property to query
        :return: Value of the requested property

        @python_v18 New function added.
        """
        return ""
    
    def ProvideKeyResponse(self, response: Union[str, bytes, bytearray]) -> str:
        """
        Provide a key response

        When a key response is received from the license server, must be sent to the DRM
        instance by using provideKeyResponse. See also GetKeyRequest.

        :param response: byte - Key data returned from the license server
        :return: A keySetId if the response is for an offline key requests which can be used later
            with restoreKeys, else return empty for streaming key requests.

        @python_v18 New function added.

        @python_v19 With python 3 the response argument must be a bytearray instead of byte.
        """
        return ""
    
    def RemoveKeys(self) -> None:
        """
        Removes all keys currently loaded in a session.

        @python_v18 New function added.
        """
        pass
    
    def RestoreKeys(self, keySetId: str) -> None:
        """
        Restores session keys stored during previous `ProvideKeyResponse` call.

        :param keySetId: string - Identifies the saved key set to restore. This value must
            never be null.

        @python_v18 New function added.
        """
        pass
    
    def SetPropertyString(self, name: str, value: str) -> None:
        """
        Set a system specific property value in the DRM system.

        :param name: string - Name of the property. This value must never be null.
        :param value: string - Value of the property to set. This value must never be null.

        @python_v18 New function added.
        """
        pass
    
    def Decrypt(self, cipherKeyId: Union[str, bytes, bytearray],
                input: Union[str, bytes, bytearray],
                iv: Union[str, bytes, bytearray]) -> bytearray:
        """
        Decrypt an encrypted data by using session keys.

        :param cipherKeyId: byte - Encryption key id (provided from a service handshake)
        :param input: byte - Cipher text to decrypt
        :param iv: byte - Initialization vector of cipher text
        :return: Decrypted input data

        @python_v18 New function added.

        @python_v19 With python 3 all arguments need to be of type bytearray instead of byte.
        """
        return bytearray()
    
    def Encrypt(self, cipherKeyId: Union[str, bytes, bytearray],
                input: Union[str, bytes, bytearray],
                iv: Union[str, bytes, bytearray]) -> bytearray:
        """
        Encrypt data by using session keys.

        :param cipherKeyId: byte - Encryption key id (provided from a service handshake)
        :param input: byte - Encrypted text
        :param iv: byte - Initialization vector of encrypted text
        :return: byte - Encrypted input data

        @python_v18 New function added.

        @python_v19 With python 3 all arguments need to be of type bytearray instead of byte.
        """
        return bytearray()
    
    def Sign(self, macKeyId: Union[str, bytes, bytearray],
             message: Union[str, bytes, bytearray]) -> bytearray:
        """
        Generate a DRM encrypted signature for a text message.

        :param macKeyId: byte - HMAC key id (provided from a service handshake)
        :param message: byte - Message text on which to base the signature
        :return: byte - Signature

        @python_v18 New function added.

        @python_v19 With python 3 all arguments need to be of type bytearray instead of byte.
        """
        return bytearray()
    
    def Verify(self, macKeyId: Union[str, bytes, bytearray],
               message: Union[str, bytes, bytearray],
               signature: Union[str, bytes, bytearray]) -> bool:
        """
        Verify the validity of a DRM signature of a text message.

        :param macKeyId: byte - HMAC key id (provided from a service handshake)
        :param message: byte - Message text on which the signature is based
        :param signature: byte - The signature to verify
        :return: true when the signature is valid

        @python_v18 New function added.

        @python_v19 With python 3 for all arguments is needed to pass bytearray instead of byte.
        """
        return True
