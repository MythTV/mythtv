# -*- coding: UTF-8 -*-

"""
MythTV Web Services for Python3.

The classes below provide easy access to the MythTV Services described at
https://www.mythtv.org/wiki/Services_API .
Currently, the following services are available:
'Capture', 'Channel', 'Content', 'Dvr', 'Frontend', 'Guide', 'Myth', 'Video'.

The definition and parameters for these services are taken from the backend or
frontend via WSDL (Web Service Description Language), e.g.:
http://mythbackend:6544/<service Name>/wsdl .
Simple caching for each service/wsdl is done per connection.

The operations as described in the Services_API and the types for each item
are extracted from this WSDL file and are used for the type conversions to the
internal python types from the MythTV Python Bindings.
For instance, automatic conversion to and from the UTC-Time is done when
receiving a time-string like '2020-04-07T17:30:00Z':
the corresponding python type will look like:
'2020-04-07 19:30:00+02:00 <class 'MythTV.utility.dt.datetime'>'

The class 'Send' from the MythTV/services_api is used to send/receive the
XML data to/from the host.
"""


# Version History:
#   1.0.0     2020-05-20 Initial
#   1.1.0     2020-05-25 Added evaluation of simple responses

__VERSION__ = "1.1.0"


import sys
if sys.version_info < (3,6):
    raise Exception("MythTV-Services support Python 3.6+ only!")

from lxml import etree
from urllib.request import urlopen
from urllib.parse import urlencode
import inspect

from MythTV.altdict import OrdDict, DictData
from MythTV.services_api import send as API
from MythTV.logging import MythLog
from MythTV.exceptions import MythError


class ParameterizedSingleton(type):
    """
    A meta-class that respects *args, **kwargs to build itself.
    If parameter match, it returns the same class, otherwise it returns
    a new one.
    """
    _instances = {}
    _init = {}

    def __init__(cls, name, bases, dct):
        cls._init[cls] = dct.get('__init__', None)

    def __call__(cls, *args, **kwargs):
        init = cls._init[cls]
        if init is not None:
            sig = inspect.signature(init)
            fset = frozenset(sig.bind(None, *args, **kwargs).arguments.items())
            key = (cls, fset)
        else:
            key = cls

        if key not in cls._instances:
            cls._instances[key] = \
                super(ParameterizedSingleton, cls).__call__(*args, **kwargs)
        return cls._instances[key]


class MythServiceCache( metaclass=ParameterizedSingleton ):
    """
    This is a singleton object for each set of 'service, host, port'.
    It creates an xsd:schema from the 'service/wsdl' file.
    It parses the soap operations from the wsdl file.
    It creates 'schema' and 'operations' as dictionaries.
    Each element of the schem_dict is an ordered dictionary of type
    'MythTV.OrdDict'.
    """
    services = ['Capture', 'Channel', 'Content', 'Dvr', 'Frontend', 'Guide',
                'Myth', 'Video']
    logmodule = 'Python Myth-Services Cache'

    def __init__(self, service, host, port=6544):
        self.service = service
        self.host = host
        self.port = port
        self.log = MythLog(self.logmodule)
        self.schema = None
        self.wsdl = None
        self.wsdl_namespaces = None
        self.wsdlroot = self._get_wsdlroot(service)
        self.schema = self._get_schema(self.wsdlroot)
        self.schema_dict = self._parse_schema(self.schema)
        self.operations_dict = {}
        self.operations = self._get_operations()
        self.log(MythLog.HTTP, MythLog.DEBUG,
                 "%s created." %self.__repr__())

    def __str__(self):
        return("MythServiceCache" + ":" + self.service)
    def __repr__(self):
        return("<%s '%s@%d' at %s>" \
                %(self.__str__(),self.host,self.port,hex(id(self))))

    def _get_wsdlroot(self, service):
        """
        Returns the root element from the wsdl and it's namespaces.

        """
        if service not in self.services:
            raise MythError("Unknown service: {0}".format(service))
        url = 'http://{hostname}:{port}/{service}/wsdl'.format(
                service=service, hostname=self.host, port=self.port)
        self.log(MythLog.HTTP, MythLog.DEBUG,
                 "Parsing wsdl '%s'." %url)
        wsdl_string = urlopen(url)
        self.wsdl = etree.parse(wsdl_string)
        root = self.wsdl.getroot()
        self.wsdl_namespaces = root.nsmap
        return(root)

    def _get_schema(self, root):
        """
        Creates a plain xsd-schema, resolves all xsd-includes.
        """
        schema = root.find(".//*[@targetNamespace='http://mythtv.org']")
        imports = {}

        def _fetch_imports_includes(root, imports):
            """
            Search through the supplied root element and fetch each import
            to be later inserted back in to the root WSDL.
            """
            import_elements = root.findall(".//*[@schemaLocation]/..")
            for import_element in list(reversed(import_elements)):
                for imp in list(reversed(import_element)):
                    location = imp.get('schemaLocation')
                    if location is None:
                        continue
                    if location in imports.keys():
                        continue
                    import_tree = etree.parse(location)
                    imports[location] = {
                        'import': imp,
                        'tree': import_tree
                        }
                    _fetch_imports_includes(import_tree, imports)
                    import_element.remove(imp)
            return

        _fetch_imports_includes(root, imports)
        for impurl, imp in imports.items():
            self.log(MythLog.HTTP, MythLog.DEBUG,
                    "Parsing 'xsd:imports' from '%s'." %impurl)
            vroot = imp['tree'].getroot()
            for e in list(vroot):
                if 'schemaLocation' in e.keys():
                    continue
                schema.insert(0,e)
        return(schema)

    def _parse_schema(self, schema):
        """
        Parses the schema specific to the service and returns a
        dictionary of dictionaries named according to schema names,
        containing the mapping to the internal type conversion facility.

        Example of sub-dictionaries: for 'Lineup':

        'GetDDLineupList': {'Password': 3, 'Source': 3, 'UserId': 3},
        'GetDDLineupListResponse': {'GetDDLineupListResult': 'LineupList'},
        'LineupList': {'Lineups': 'ArrayOfLineup'},
        'ArrayOfLineup': ['Lineup'],
        'Lineup': {'Device': 3,
                   'DisplayName': 3,
                   'LineupId': 3,
                   'Name': 3,
                   'Postal': 3,
                   'Type': 3},
        """

        def _get_type(xin, nillable=False):
            """
            Do the conversion between MythTV's internal types and
            the xsd-types according the class 'DictData'.
            """
            lookup_dict = { "xs:int":         0,
                            "xs:unsignedInt": 0,
                            "xs:long":        0,
                            "xs:double":      1,
                            "xs:float":       1,
                            "xs:boolean":     2,
                            "xs:string":      3,
                            "xs:date":        5,
                            "xs:dateTime":    7,
                            "xs:time":        8
                          }

            ### add nillable types for optional parameters, once the wsdl support them
            if xin in lookup_dict:
                return(lookup_dict[xin])
            else:
                raise MythError("Unknown key: {0}".format(xin))

        def _parse_subtree(item):
            d_list = []
            element_dict = OrdDict()
            for t in item.iter():
                if item.tag.endswith('simpleType'):
                    if t.tag.endswith('enumeration'):
                        enumvalue = enumkey = None
                        for te in t.iter():
                            if te.tag.endswith('EnumerationValue'):
                                enumvalue = int(te.text.strip())
                            elif te.tag.endswith('EnumerationDesc'):
                                enumdict, enumkey = te.text.rsplit('.',1)
                            if enumvalue is not None and enumkey is not None:
                                element_dict[enumkey] = enumvalue
                elif item.tag.endswith('complexType'):
                    if t.tag.endswith('element'):
                        if t.get('type') is not None:
                            type_ns, type_value = t.get('type').split(':')
                            if (t.get('maxOccurs', '') == 'unbounded'):
                                if type_ns == 'xs':
                                    # basic type, add conversion rule for the elements
                                    t_value = _get_type(t.get('type'),
                                              t.get('nillable', None))
                                else:
                                    # add arrays of type 'tns' immediately
                                    t_value = type_value
                                d_list.append({item.get('name') : [t_value]})
                                continue
                            elif (type_ns == 'xs'):
                                element_dict[t.get('name')] = \
                                                        _get_type(t.get('type'),
                                                        t.get('nillable', None))
                            else:
                                element_dict[t.get('name')] = type_value
                        else:
                            pass
                elif item.tag.endswith('element'):
                    if t.tag.endswith('element'):
                        if t.get('type') is not None:
                            type_ns, type_value = t.get('type').split(':')
                            if (type_ns == 'xs'):
                                element_dict[t.get('name')] = \
                                                       _get_type(t.get('type'),
                                                       t.get('nillable', None))
                            else:
                                element_dict[t.get('name')] = type_value

            if (len(element_dict) > 0):
                d_list.append({item.get('name') : element_dict})
            else:
                ### special case: item does not have any children
                if not item.get('name').startswith('ArrayOf'):
                    d_list.append({item.get('name') : element_dict})
            return(d_list)

        schema_dict = {}
        xsd_types_to_parse = ['simpleType', 'complexType', 'element']
        for xsd_type in xsd_types_to_parse:
            typelist = self.schema.findall("{%s}%s" %(self.wsdl_namespaces['xs'],
                                                      xsd_type))
            typelist = sorted(typelist, key = lambda x: x.get('name'))
            for t in typelist:
                if (t.get('name') not in schema_dict):
                    for sdict in _parse_subtree(t):
                        schema_dict.update(sdict)
                        self.log(MythLog.HTTP, MythLog.DEBUG,
                                "Adding dictionary to schema-dict:",
                                detail = sdict)
        return(schema_dict)

    def _get_operations(self):
        """
        Returns a list of Dictionaries for all operations of this service.
        """
        ops = []
        wsdl_ops = self.wsdl.findall('.//operation/documentation/..',
                                     self.wsdl_namespaces)
        for op in wsdl_ops:
            opname = op.get('name')
            if opname is not None:
                ops.append(opname)
                op_dict = {}
                op_dict['opname']  = opname
                for i in op.getchildren():
                    if 'documentation' in i.tag:
                        msg_type = i.text.strip()
                        op_dict['optype'] = msg_type
                        op_dict['opdata'] = self.schema_dict['%s' %opname]
                self.operations_dict.update({opname : op_dict})
                self.log(MythLog.HTTP, MythLog.DEBUG,
                        "Adding dictionary to operations-dict:",
                                 detail = {opname : op_dict})
            else:
                raise MythError("Operation without name: {0}".format(op))
        return(ops)


class MythServiceData( DictData ):
    """
    Creates an instance of MythServiceCache(service, host, port),
    and mirrors the dictionaries for 'schema' and for 'operations'.
    Provides low-level access to the data of MythTV Web Services
    out of lxml Etree data, by mapping to the MythTV internal python types.
    """

    _field_type = 'Pass'
    _field_order = []
    logmodule = 'Python Myth-Services'

    def __init__(self, service, host, port=6544, xml_etree=None, schema_tag=None):
        dict.__init__(self)
        self.data = []
        self._field_type = []
        self._field_order = []
        self.service = service
        self.host = host
        self.port = port
        self.log = MythLog(self.logmodule)
        cached = MythServiceCache(service, host, port)
        self.schema_dict = cached.schema_dict
        self.schema = cached.schema
        self.operations_dict = cached.operations_dict
        self.operations = cached.operations
        self.wsdl = cached.wsdl
        self.operation_version = None
        if xml_etree is not None:
            self.perform(xml_etree,schema_tag=schema_tag)

    def perform(self, xml_etree, schema_tag=None):
        self.xml_root = self.fromEtree(xml_etree, schema_tag=schema_tag)
        self.operation_version = self.xml_root.get("version")
        self._process(self.data)

    def fromEtree(self, xml_etree, schema_tag=None):
        """
        Recursively parse the given element tree and prepares the 3 lists
        required by the DictData class for building an object for the
        MythTV Python Bindings:
        - self.data: holds the actual values of each field (as strings)
        - self._field_order: holds the keys for each field
        - self._field_type: holds the index to the transition function
        Shorcuts are used for the various usages of the 'ArrayOf' lists,
        the 'MapOf' dictionaries and the ".Type" enums.
        Note: 'MapfOf' is a list of tuples (key, value), which is
        converted to a dictionary, by the dict() class.
        """
        root = xml_etree
        if schema_tag is not None:
            root_name = schema_tag
        else:
            root_name = xml_etree.tag
        for child in root.getchildren():
            # child.text is either a string, or 'None'
            if child.tag in self.schema_dict[root_name]:
                tvalue = self.schema_dict[root_name][child.tag]
                if tvalue in self.schema_dict.keys():
                    # handle 'ArrayOf', 'MapOf', '.Type' explicitely
                    if tvalue.startswith('ArrayOf'):
                        arr = []
                        if isinstance(self.schema_dict[tvalue][0], int):
                            # array of basic type 'xs', type already known:
                            for c in child.getchildren():
                                arr.append(c.text)
                        else:
                            for c in child.getchildren():
                                i = MythServiceData(self.service,
                                                    self.host,
                                                    port=self.port,
                                                    xml_etree=c)
                                arr.append(i)
                        self.data.append(arr)
                        self._field_order.append(child.tag.lower())
                        # 'ArrayOf' has it's own type representation:
                        self._field_type.append(9)                  # hardcoded
                    elif tvalue.startswith('MapOf'):
                        arr = []
                        for c in child.getchildren():
                            key = value = None
                            for cc in c.getchildren():
                                if cc.tag == "Key":
                                    key = cc.text
                                if cc.tag == "Value":
                                    value = cc.text
                            if key is not None and value is not None:
                                arr.append((key, value))
                        self.data.append(dict(arr))
                        self._field_order.append(child.tag.lower())
                        # 'MapfOf' is a list of tuples with representation:
                        self._field_type.append(10)                 # hardcoded
                    elif tvalue.endswith('.Type'):
                        ## Note: this would add the enum value to data:
                        #  self.data.append(self.schema_dict[tvalue][child.text])
                        #  self._field_type.append(0)               # hardcoded
                        # this adds the enum key to data:
                        self.data.append(child.text)
                        self._field_order.append(child.tag.lower())
                        self._field_type.append(3)                  # hardcoded
                    else:
                        # recurse to that etree
                        child_schema_tag = self.schema_dict[root_name][child.tag]
                        i = MythServiceData(self.service,
                                            self.host,
                                            port=self.port,
                                            xml_etree=child,
                                            schema_tag=child_schema_tag)
                        self._field_order.append(child.tag.lower())
                        self.data.append(i)
                        # hardcoded for string, but it is a 'class'
                        self._field_type.append(3)
                else:
                    # tvalue is an integer index to the type
                    self.data.append(child.text)
                    self._field_order.append(child.tag.lower())
                    self._field_type.append(tvalue)
            else:
                # child not found in schema_dict
                raise MythError("Cannot find entry of '%s' in schema dictionary!"
                                 %child.tag)
        return(root)

    def _process(self, data):
        data = DictData._process(self, data)
        DictData.update(self, data)

    def getdefault(self, item):
        """
        Recursively set default values for an item.
        Item may be a dict or a single element from 'self.schema_dict'.
        """
        d = {}
        for k, v in item.items():
            if isinstance(v, dict):
               d[k] = self.getdefault(v)
            elif isinstance(v, int):
               d[k] = self._defaults[v]
            else:
               d[k] = v
        return(d)

    def getencoded(self, sdict, item):
        """
        Recursively set encoded values, i.e.: string values for an item.
        Item may be a dict or a single element from 'self.schema_dict'.
        """
        d = {}
        for k, v in item.items():
            if isinstance(v, dict):
               d[k] = self.getencoded(sdict[k],v)
            elif isinstance(sdict[k], int):
               d[k] = self._inv_trans[sdict[k]](v)
               # if destination is a string, convert boolean to string
               if ((sdict[k]==3) and isinstance(d[k], bool)):
                   d[k] = str(int(d[k]))
            else:
               d[k] = v
        return(d)

    def stripdefault(self, defitem, item ):
        """
        Recursively strip off default values of an item.
        Item may be a dict or a a single element from 'self.schema_dict'.
        """
        d = {}
        for k, v in defitem.items():
            if isinstance(v, dict):
               d[k] = self.stripdefault(v, item[k])
            elif isinstance(v, int):
               if item[k] != self._defaults[v]:
                   d[k] = item[k]
            else:
               d[k] = v
        return(d)


class ServiceAPI( object ):
    """Adds a context handler for opening/closing a session of
       the 'services_api.Send' class.
    """
    def __init__(self, host, port=None):
        self.host = host
        self.port = port
        self.mythrequest = API.Send(host, port=port)

    def __enter__(self):
        return self.mythrequest.send

    def __exit__(self, exception_type, exception_value, traceback):
        self.mythrequest.close_session()


class MythTVService( MythServiceData ):
    """
    User Interface to the MythTV's Services:

    This class can be used in two ways:

    Either to be called directly with all parameters to perform an operation:
    Each operation consists of a call to - and a reception of - the selected
    MythTVService. A direct call uses at least the following parameters:
      - service: the service category one is asking for (e.g.: 'Dvr'
        optype = 'POST' # or 'GET'
      - opdata: a dictionary describing the data needed to perform the
                operation.
    The result of this direct call is stored in 'self.operation_result'.

    Or 'MythTVService' is used sequentially by calling:
      - getoperation
      - strip_operation_defaults
      - encode_operation
      - perform_operation
    An instance of the 'MythTVService' class can be reused to perform multiple
    operations of the same service type.

    Example of an operation:
      ms = MythTVService('Channel', host)
      op = ms.getoperation('GetChannelInfoList')
      op['opdata']['SourceID'] = 1
      op['opdata']['Details'] = True
      op_stripped = ms.strip_operation_defaults('GetChannelInfoList', op)
      op_encoded = ms.encode_operation(op_stripped)
      if ms.perform_operation(op_encoded):
            # 'ms' is now an object of type 'GetChannelInfoList'
            print(ms.channelinfos[0].chanid)
      print(ms.operation_version)

    Note: The 'operation_version' attribue is only available for complex
          operations.

    Notes about naming convention:
    The MythTV Python Bindings use lower-case names for their attributes.
    The MythTV-Services API use 'CamelCase' notation for the data needed by
    'POST' and returned by 'GET' operations. During processing of these data,
    they are converted to lower-case: Please note, that Python's PEP-08
    dislikes 'CamelCase' notation for naming attributes of a class.

    Since every call to 'MythTVService' returns an instance derived from the
    base-class 'DictData', one can access these data by either using a
    'dictionary-like notation', or using the 'key as attribute'.

    TL;DR: Attributes are lower-case names and the following notations
    belong to the same object:
        - ms.channelinfo.chanid
        - ms.channelinfo['chanid']
        - ms['channelinfo']['chanid']

    A list of available attributes is available by calling
    'ms.list_attributes()'.

    Notes about logging:
    This module uses the internal logging mechanism from 'logging.py'.
    To enable all debug logging to console, start the python script with
    $ python3 your_script.py --loglevel debug --verbose all .
    For the interactive python shell, replace the scriptname with a dash ("-").

    See 'MythTV/services_api/send.py' for logging of the services_api.
    """

    def __init__(self, service, host, port=6544, opname=None, optype=None, timeout=None, **opdata):
        self.service = service
        self.host = host
        self.port = port
        self.operation = None
        # set timeout globally for that session:
        self.operation_timeout = timeout
        self.operation_result = None
        MythServiceData.__init__(self, service, host, port=port)
        # initialize connection via MythServiceAPI:
        self.request = ServiceAPI(host, port)
        # evaluate operation kwargs (needs python 3.6+):
        if opname and optype:
            op = {}
            try:
                op["opname"] = opname
                op["optype"] = optype.upper()
                if opdata:
                    op["opdata"] = opdata
                else:
                    op["opdata"] = {}
                opdict = self.encode_operation(op)
                self.perform_operation(opdict, timeout=timeout)
            except:
                raise MythError("Unable to perform operation '%s': '%s': '%s'!"
                                %(opname, optype, opdata))

    def __str__(self):
        return "{0.__class__.__name__}({0.service}:{0.operation})".format(self)

    def __repr__(self):
        return(self.__str__() + " at %s>" % (hex(id(self))))

    def encode_operation(self, opdict):
        """
        This translates the MythtV Python types to their string equivalents:
          - 'bool' types --> '0' or '1'
          - 'datetime' types --> 'YYYY-MM-DDThh:mm:ssZ' aka ISO-UTC time
          - 'int', 'float' types --> string
          ...
        """
        op = opdict['opname']
        return(self.getencoded(self.operations_dict[op], opdict))

    def get_operation(self, op):
        """
        Returns a dictionary of data needed to perform a call to 'ServiceAPI'.
        Parameters for a 'GET' operation are initialized with default values.
        'POST' data are set up with default values.
        """
        self.operation = op
        return(self.getdefault(self.operations_dict[op]))

    def _eval_response(self, eroot):
        """
        Evaluate xml data and transform them to the correct data types:
        Fill the DictData object with xml data:
        - self.data: holds the actual values of each field (as strings)
        - self._field_order: holds the keys for each field
        - self._field_type: holds the index to the transition function
        """
        response = self.schema_dict["%sResponse"%self.operation]
        result_type = response["%sResult"%self.operation]
        result_name = self.operation.replace("Get", "").lower()
        res = False
        if isinstance(result_type, int):
            # basic type, transform it:
            self.data.append(eroot.text)
            self._field_order.append(result_name)
            self._field_type.append(result_type)
            res = True
        else:
            try:
                # check if it is an array
                if result_type.lower().startswith("arrayof"):
                    data = []
                    result_type_index = self.schema_dict[result_type][0]
                    for el in eroot:
                        data.append(self._trans[result_type_index](el.text))
                    self.data.append(data)
                    self._field_order.append(result_name)
                    self._field_type.append(9)             # hardcoded for a list
                    res = True
            except AttributeError:
                raise MythError("Unknown type of result: {0}".format(result_type))
        if res:
            self._process(self.data)
        return(res)

    def perform_operation(self, opdict, timeout=None):
        """
        Actually performs the operation given by a dictionary.
        like http://<hostip>:6544/Dvr/GetRecordedList?Descending=True&Count=3
        The xml data retrieved from the host are of different types:
        - a simple response containing only values or list of values,
          like the response to 'GetHostName' or 'GetHosts'.
          Simple responses get evaluated and are returned directly.
        - a complex response containing a xml namespace and a version,
          like the response to 'ChannelInfo' or 'ChannelInfoList'.
          Complex responses will be objectified within this class.
          The version of these responses is stored in 'self.operation_version'.
        The result of this method is stored in 'self.operation_result'.
        Optionally, a 'timeout' value can be set for this operation.
        """
        self.operation = opdict['opname']
        messagetype = opdict['optype']
        # define session options valid for 'POST' and 'GET' operations:
        opts = {'rawxml': True}
        if self.operation_timeout:
            opts['timeout'] = self.operation_timeout
        if timeout:
            opts['timeout'] = timeout

        with self.request as api_request:
            result = None
            if messagetype == 'GET':
                self.log(MythLog.HTTP, MythLog.INFO, "Perform Operation"
                        "'%s': 'GET': %s" %(self.operation, opdict['opdata']))
                endpoint = '%s/%s'%(self.service, self.operation)
                if opdict['opdata'] is not None:
                    rest = urlencode(opdict['opdata'])
                else:
                    rest = None
                try:
                    result = api_request(endpoint=endpoint, rest=rest, opts = opts)
                except RuntimeWarning as warning:
                    # Could be an image:
                    if str(warning).startswith("Image file"):
                       r = str(warning).replace('Image file = ','')
                       result = r.strip('"')
                       self.operation_result = result
                       return(result)
                    else:
                       raise MythError("Unknown result from 'send' operation")
                except RuntimeError as e:
                    raise MythError(e.args)
                eroot = etree.fromstring(result)
                #print(etree.tostring(eroot, pretty_print=True, encoding='unicode'))
                if eroot.tag in self.schema_dict.keys():
                    self.perform(eroot)
                    result = True
                else:
                    result = self._eval_response(eroot)
            elif messagetype == 'POST':
                endpoint = '%s/%s'%(self.service, self.operation)
                opts['wrmi'] = True
                if opdict['opdata'] is not None:
                    post = opdict['opdata']
                else:
                    post = None
                try:
                    self.log(MythLog.HTTP, MythLog.INFO, "Perform Operation"
                            "'%s': 'POST': %s" %(self.operation, opdict['opdata']))
                    result = api_request(endpoint=endpoint, postdata=post, opts = opts)
                    # result is of type "<bool>true</bool>" or similar "<int>1</int>"
                    el = etree.fromstring(result)
                    # translate the result to the known type of the schema
                    # the "Response" message is  used in all wsdls the same way
                    out_msg = opdict['opname']+"Response"
                    k, v = self.schema_dict[out_msg].items()[0]
                    result = self._trans[v](el.text)
                except:
                    raise MythError("Unknown post data :{0}".format(post))
            else:
                raise MythError("Unknown operation for {0}".format(messagetype))
            self.operation_result = result
            return(result)

    def strip_operation_defaults(self, operation, op):
        """
        Compares an 'operations_dict' element with the given
        element and strip off the defaults from the given element.
        """
        org_op = self.operations_dict[operation]
        return(self.stripdefault(org_op, op))

    def get_pages(self, opdict, itemlist):
        """
        Returns an iterator over all 'itemlist' objects available by
        the operation defined by the oerations_dict.
        The index is the key "StartIndex', the page size is defined
        by 'Count'.
        The end condition is given by the 'TotalAvailable' value returned
        from the operation.
        """
        def __logme(start, count, s_startindex, s_totalavailable):
            self.log(MythLog.HTTP, MythLog.INFO, "Get Pages: start=%s : "
                "count=%s : s.startindex=%s : s.totalavailable=%s"
                 %(start, count, s_startindex, s_totalavailable))

        itemlist = itemlist.lower()   # InCaseOfCamelCaseNotation
        start = opdict['opdata']['StartIndex']
        count = opdict['opdata']['Count']
        # perform the operation the first time
        optdict_enc = self.encode_operation(opdict)
        result = self.perform_operation(optdict_enc)
        yield self[itemlist]
        start = start + count
        while ((start <= self.totalavailable) and result):
            opdict['opdata']['StartIndex'] = start
            __logme(start, count, self.startindex, self.totalavailable)
            optdict_enc = self.encode_operation(opdict)
            result = self.perform_operation(optdict_enc)
            yield self[itemlist]
            start = start + count
        if not result:
            self.log(MythLog.HTTP, MythLog.ERR, "Get Pages on '%s' failed."
                                                % itemlist)

    # some helper methods:
    def list_attributes(self, item=None):
        """
        Returns a list of attributes of an already performed operation.
        """
        if item:
            i = self[item]
            if isinstance(i,list):
                return(i[0]._field_order)
            else:
                return(i._field_order)
        else:
            return(self._field_order)

    def list_types(self, stype=None):
        """
        Returns a list of attributes and their types of an already
        performed operation.
        """
        l = []
        if stype:
            for i in stype._field_order:
                l.append("%s : %s : %s" %(i, stype[i], type(stype[i])))
        else:
            for i in self._field_order:
                l.append("%s : %s : %s" %(i, self[i], type(self[i])))
        return(l)

    def list_operations(self):
        """
        Returns a list of available operations, (i.e.: calls to MythService-API).
        """
        return(self.operations)

    def list_schema(self):
        """
        Prints the xsd schema of the selected MythTV-Service.
        """
        return(etree.tostring(self.schema, pretty_print=True, encoding='unicode'))

