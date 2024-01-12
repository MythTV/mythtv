Kodistubs
=========
(former xbmcstubs)
------------------

.. image:: https://badge.fury.io/py/Kodistubs.svg
    :target: https://badge.fury.io/py/Kodistubs

Kodi stubs are Python files that can help you develop addons for `Kodi (XBMC)`_ Media Center.
Use them in your favorite IDE to enable autocompletion and view docstrings
for Kodi Python API functions, classes and methods.
Kodistubs also include `PEP-484`_ type annotations for all functions
and methods.

You can install Kodistubs into your working virtual environment using ``pip``::

    $ pip install Kodistubs

Read `Kodistubs documentation`_ for more info on how to use Kodi stubs.

Kodistubs major version corresponds to the version of Kodi they have been generated from.
Current Kodistubs are compatible with Python 3.6 and above. Kodistubs for Kodi versions that used
Python 2.x for addons can be found in ``python2`` branch.

**Warning**: Kodistubs are literally stubs and do not include any useful code,
so don't try to run your program outside Kodi unless you add some testing code into Kodistubs
or use some mocking library to mock Kodi Python API.

Current Kodistubs have been generated from scratch using `this script`_ from Doxygen XML files and
SWIG XML Python binding definitions that, in their turn, have been generated
from Kodi sources. Old Kodistubs can be found in ``legacy`` branch.

I try to keep Kodi stubs in sync with Kodi Python API development, but it may happen
that I miss something. Don't hesitate to open issues or submit pull requests if you notice
discrepancies with the actual state of Kodi Python API.


`Discussion topic on Kodi forum`_

License: `GPL v.3`_

.. _Kodi (XBMC): http://kodi.tv
.. _Discussion topic on Kodi forum: http://forum.kodi.tv/showthread.php?tid=173780
.. _GPL v.3: http://www.gnu.org/licenses/gpl.html
.. _Kodistubs documentation: https://romanvm.github.io/Kodistubs/
.. _PEP-484: https://www.python.org/dev/peps/pep-0484/#suggested-syntax-for-python-2-7-and-straddling-code
.. _this script: https://github.com/romanvm/kodistubs-generator
