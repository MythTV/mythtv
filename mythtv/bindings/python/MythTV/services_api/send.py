# -*- coding: utf-8 -*-

"""API Client."""

import json
import re
import sys
import tempfile
import logging
from os import fdopen
from lxml import etree,html

try:
    import requests
except ImportError:
    sys.exit('Install python-requests or python3-requests')

from ._version import __version__
from .mythversions import MYTHTV_VERSION_LIST



# pylint: disable=too-many-instance-attributes
class Send():
    """Services API."""

    def __init__(self, host, port=6544):
        """
        INPUT:
        ======

        host:     Must be set and is the hostname or IP address of the backend
                  or frontend.

        port:     Only needed if the backend is using a different port
                  (unlikely) or set to the frontend port, which is usually
                  6547. Defaults to 6544.
        """

        if not host:
            raise RuntimeError('Missing host argument')

        self.host = host
        self.port = port
        self.endpoint = None
        self.jsondata = None
        self.postdata = None
        self.rest = None
        self.session_token = None
        self.opts = None
        self.session = None
        self.server_version = 'Set to MythTV version after calls to send()'
        self.logger = logging.getLogger(__name__)

        logging.getLogger(__name__).addHandler(logging.NullHandler())

    # pylint: disable=too-many-arguments,too-many-statements
    def send(self, endpoint='', postdata=None, jsondata=None, rest='',
             opts=None):
        """
        Form a URL and send it to the back/frontend.  Parameter/option checking
        and session creation (if required) is done here. Error handling is done
        here too.

        EXAMPLES:
        =========

        import MythTV.services_api.send as send

        backend = send.Send(host='someName')
        backend.send(endpoint='Myth/GetHostName')

        Returns: {'String': 'someBackend'}

        frontend = send.Send(host='someFrontend', port=6547)
        frontend.send(endpoint='Frontend/GetStatus')

        Returns: {'FrontendStatus': {'AudioTracks': {}, 'Name': 'someHost', ...

        INPUT:
        ======

        endpoint: Must be set. Example: Myth/GetHostName

        postdata: May be set if the endpoint allows it. Used when information
                  is added/changed/deleted. postdata is passed as a Python
                  dict e.g. {'ChanId':1071, ...}. Don't use if rest is used.
                  The HTTP method will be a POST (as opposed to a GET.)

                  If using postdata, TAKE CAUTION!!! Use opts['wrmi']=False
                  1st, turn on DEBUG level logging and then when happy with
                  the data, make opts['wrmi']=True.

                  N.B. The MythTV Services API is still evolving and the wise
                  user will backup their DB before including postdata.

        rest:     May be set if the endpoint allows it. For example, endpoint=
                  Myth/GetRecordedList, rest='Count=10&StorageGroup=Sports'
                  Don't use if postdata is used. The HTTP method will be a GET.

        opts (Short Description):

        It's OK to call this function without any options set and:

            . If there's postdata, nothing will be sent to the server
            . timeout will be set to 10 seconds
            . It will fail if the backend requires authorization (user/pass
              would be required)

        opts (Details):

        opts is a dictionary of options that may be set in the calling program.
        Default values will be used if callers don't pass all or some of their
        own. The defaults are all False except for 'user', 'pass' and
        'timeout'.

        opts['noetag']:  Don't request the back/frontend to check for matching
                         ETag. Mostly for testing.

        opts['nogzip']:  Don't request the back/frontend to gzip it's response.
                         Useful if watching protocol with a tool that doesn't
                         uncompress it.

        opts['timeout']: May be set, in seconds. Examples: 5, 0.01. Used to
                         prevent script from waiting indefinitely for a reply
                         from the server. Note: a timeout exception is only
                         raised if there are no bytes received from the host on
                         this socket. Long downloads are not affected by this
                         option. Defaults to 10 seconds.

        opts['user']:    Authentication. Optionally turned on via the Web App.
        opts['pass']:    Available in v36-Pre. [Was for digest authentication]

        opts['usexml']:  For testing only! If True, causes the backend to send
                         its response in XML rather than JSON. Defaults to
                         False.

        opts['rawxml']:  If True, causes the backend to send it's response in
                         XML as bytes. This can be easily parsed by Python's
                         'lxml.etree.fromstring()'. Defaults to False.

        opts['wrmi']:    If True and there is postdata, the URL is then sent to
                         the server.

                         If opts['wrmi'] is False and there is postdata, send
                         NOTHING to the server.

                         This is a fail-safe that allows testing. Users can
                         examine what's about to be sent before doing it (wrmi
                         means: We Really Mean It.)

        opts['wsdl']:    If True return WSDL from the back/frontend. Accepts no
                         rest or postdata. Just set endpoint, e.g. Content/wsdl

        OUTPUT:
        =======

        Either the response from the server in the selected format (default is
        JSON.) Or an exception of RuntimeWarning or RuntimeError.

        Callers can handle the response like this:

            backend = send.Send(host=...)

            try:
                response = backend.send(...)
            except RuntimeError as error:
                handle error...
            except RuntimeWarning as warning:
                handle warning...

            normal processing...

        If an Image filename is returned, then the caller is responsible for
        deleting the temporary filename which is returned in a RuntimeWarning
        exception, e.g.:

            'Image file = "/tmp/tmp5pxynqdf.jpeg"'

        However, some errors returned by the server are in XML, e.g. if an
        endpoint is invalid. That will cause the JSON decoder to fail. In
        the application calling this, turn logging on and use the DEBUG
        level. See the next section.

        Whenever send() is used, 'server_version' is set to the value returned
        by the back/frontend in the HTTP Server: header. It is saved as just
        the version, e.g. 0.28. Callers can check it and *may* choose to adjust
        their code work with other versions. See: get_server_version().

        LOGGING:
        ========
        Callers may choose to use the Python logging module. Lines similar to
        the following will make log() statements print if the level is set to
        DEBUG:

            import logging

                logging.basicConfig(level=logging.DEBUG
                                    if args['debug'] else logging.INFO)
                logging.getLogger('requests.packages.urllib3')
                                  .setLevel(logging.WARNING)
                logging.getLogger('urllib3.connectionpool')
                                  .setLevel(logging.WARNING)

        """

        self.endpoint = endpoint
        self.postdata = postdata
        self.jsondata = jsondata
        self.rest = rest
        self.opts = opts

        self._set_missing_opts()

        url = self._form_url()

        self.logger.debug('URL=%s', url)

        if self.session is None:
            self._create_session()

        if self.postdata:
            self._validate_postdata()

        ##############################################################
        # Actually try to get the data and handle errors.            #
        ##############################################################

        try:
            if self.postdata is not None or self.jsondata is not None:
                response = self.session.post(url, data=self.postdata,
                                             json=self.jsondata,
                                             timeout=self.opts['timeout'])
            else:
                response = self.session.get(url, timeout=self.opts['timeout'])
        except SessionException().errors as session_errors:
            raise RuntimeError(f'Connection problem/Keyboard Interrupt, '
                               f'URL={url}') from session_errors

        if response.status_code == 401:
            raise RuntimeError('Unauthorized (401). Set valid user/pass opts.')

        # TODO: Should handle redirects here (mostly for remote backends.)
        if response.status_code > 299:
            self.logger.debug('%s', response.text)
            try:
                doc = html.fromstring(response.text)
            except etree.ParserError:
                reason = 'N/A'
            else:
                reason = doc.findtext('.//h1')
            raise RuntimeError(f'Unexpected status code: '
                               f'{response.status_code}, Reason: "{reason}", '
                               f'URL: {url}')

        self._validate_header(response.headers['Server'])

        self.logger.debug('Response headers: %s', response.headers)

        if response.encoding is None:
            response.encoding = 'UTF8'

        try:
            ct_header, image_type = response.headers['Content-Type'].split('/')
        except (KeyError, ValueError):
            ct_header = None

        ##############################################################
        # Finally, return the response in the desired format         #
        ##############################################################

        if self.opts['wsdl']:
            return {'WSDL': response.text}

        if ct_header == 'image':
            handle, filename = tempfile.mkstemp(suffix='.' + image_type)
            self.logger.debug('created %s, remember to delete it.', filename)
            with fdopen(handle, 'wb') as f_obj:
                for chunk in response.iter_content(chunk_size=8192):
                    f_obj.write(chunk)
            raise RuntimeWarning(f'Image file = "{filename}"')

        try:
            self.logger.debug('1st 60 bytes of response: %s',
                              response.text[:60])
        except UnicodeEncodeError:
            pass

        if self.opts['usexml']:
            return response.text

        if self.opts['rawxml']:
            return response.content

        try:
            return response.json()
        except ValueError as err:
            raise RuntimeError(f'Set loglevel=DEBUG to see JSON parse error: '
                               f'{err}') from err

    def close_session(self):
        """
        This is here for unit tests that need to start a new session
        so that noetag, nogzip and usexml (that were configured in
        _create_session()) can be changed.
        """

        self.session.close()

    def _set_missing_opts(self):
        """
        Sets options not set by the caller to False (or 10 in the
        case of timeout.
        """

        if not isinstance(self.opts, dict):
            self.opts = {}

        for option in ('noetag', 'nogzip', 'usexml', 'rawxml', 'wrmi', 'wsdl'):
            try:
                self.opts[option]
            except (KeyError, TypeError):
                self.opts[option] = False

        try:
            self.opts['timeout']
        except KeyError:
            self.opts['timeout'] = 10

        self.logger.debug('opts=%s', self.opts)

    def _form_url(self):
        """ Do basic sanity checks and then form the URL. """

        if self.host == '':
            raise RuntimeError('No host name.')

        if not self.endpoint:
            raise RuntimeError('No endpoint (e.g. Myth/GetHostName.)')

        if self.postdata and self.rest:
            raise RuntimeError('Use either postdata or rest, not both.')

        if self.opts['wsdl'] and self.rest:
            raise RuntimeError('usage: rest not allowed with WSDL')

        if not self.rest:
            self.rest = ''
        else:
            self.rest = '?' + self.rest

        return f'http://{self.host}:{self.port}/{self.endpoint}{self.rest}'

    def _validate_postdata(self):
        """
        Return a RuntimeError if the postdata passed doesn't make sense. Call
        this only if there is postdata.
        """

        if not isinstance(self.postdata, dict):
            raise RuntimeError('usage: postdata must be passed as a dict')

        self.logger.debug('The following postdata was included:')
        for k, v in self.postdata.items():
            self.logger.debug('%15s: %s', k, v)

        if not self.opts['wrmi']:
            raise RuntimeWarning('wrmi=False')

        if self.opts['wsdl'] and self.postdata:
            raise RuntimeError('usage: postdata not allowed with WSDL')

    def _create_session(self):
        """
        Called if a session doesn't already exist. Sets the desired
        headers and provides for authentication.
        """

        self.session = requests.Session()
        self.session.headers.update({'User-Agent': 'Python Services API '
                                     f'v{__version__}'})
        self.session.headers.update({'Accept': 'application/json'})

        self._get_api_session_token()
        if self.session_token:
            self.session.headers.update({'authorization': self.session_token})

        if self.opts['noetag']:
            self.session.headers.update({'Cache-Control': 'no-store'})
            self.session.headers.update({'If-None-Match': ''})

        if self.opts['nogzip']:
            self.session.headers.update({'Accept-Encoding': ''})
        else:
            self.session.headers.update({'Accept-Encoding': 'gzip,deflate'})

        if self.opts['usexml'] or self.opts['rawxml']:
            self.session.headers.update({'Accept': ''})

        self.logger.debug('New session')

    def _get_api_session_token(self):
        """ v36+ replaces original digest authentication.  """

        try:
            login_postdata = {}
            login_postdata['UserName'] = self.opts['user']
            login_postdata['Password'] = self.opts['pass']
        except KeyError:
            self.session_token = None
            return

        url = f'http://{self.host}:{self.port}/Myth/LoginUser'

        try:
            response =  self.session.post(url, data=login_postdata,
                                          timeout=self.opts['timeout'])
        except SessionException().errors as session_errors:
            raise RuntimeError(f'Connection problem/Keyboard Interrupt, '
                               f'URL={url}') from session_errors

        if response.status_code > 399:
            raise RuntimeError(f'Authorization Failed. Need valid user/pass. '
                               f'Got Status Code={response.status_code}')

        token = json.loads(response.text)

        self.session_token = token["String"]

    def _validate_header(self, header):
        """
        Process the contents of the HTTP Server: header. Try to see
        what version the server is running on. The tested versions
        are kept in MYTHTV_VERSION_LIST and checked against responses
        like:

            MythTV/30-Pre-9-g1234567-dirty Linux/3.13.0-85-generic UPnP/1.0.
            MythTV/29-pre-5-g6865940-dirty Linux/3.13.0-85-generic UPnP/1.0.
            MythTV/0.28.0-10-g57c1afb Linux/4.4.0-21-generic UPnP/1.0.
            Linux 3.13.0-65-generic, UPnP/1.0, MythTV 0.27.20150622-1
        """

        if not header:
            raise RuntimeError(f'No HTTP Server header returned from host '
                               f'{self.host}.')

        self.logger.debug('Received Server: %s', header)

        for version in MYTHTV_VERSION_LIST:
            if re.search('MythTV.' + version, header):
                self.server_version = version
                return

        raise RuntimeError(f'Tested on {MYTHTV_VERSION_LIST}, not: {header}.')

    @property
    def get_server_version(self):
        """
        Returns the version of the back/frontend. Only works after send()
        has been called.
        """
        return self.server_version

    @property
    def get_opts(self):
        """ Returns all opts{}, whether set manually or automatically. """
        return self.opts

    def get_headers(self, header=None):
        """
        Returns the requested header or all headers if none is specified.
        These are the headers sent, not those received from the backend.
        """

        if not self.session.headers:
            self.logger.debug('No headers yet, call send() 1st.')
            return None

        if not header:
            return self.session.headers

        return self.session.headers[header]


class SessionException():
    """ Things that can go wrong with POSTs or GETS used here. """

    def __init__(self):
        self.errors = (requests.exceptions.HTTPError,
                       requests.exceptions.URLRequired,
                       requests.exceptions.Timeout,
                       requests.exceptions.ConnectionError,
                       requests.exceptions.InvalidURL,
                       KeyboardInterrupt)

# vim: set expandtab tabstop=4 shiftwidth=4 smartindent noai colorcolumn=80:
