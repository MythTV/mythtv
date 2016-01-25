Libdiscid Python bindings
-------------------------

Python-discid implements Python bindings for MusicBrainz Libdiscid. This
module works with Python 2 >= 2.6, or Python 3 >= 3.1.
Libdiscid >= 0.2.2 is needed.

Libdiscid's main purpose is the calculation of an identifier of audio
discs (disc ID) to use for the `MusicBrainz database <http://musicbrainz.org>`_.

That identifier is calculated from the TOC of the disc, similar to the
freeDB CDDB identifier. Libdiscid can calculate MusicBrainz disc IDs and
freeDB disc IDs.
Additionally the MCN of the disc and ISRCs from the tracks can be extracted.

This module is a close binding that offloads all relevant data
storage and calculation to Libdiscid. On the other hand it gives a
pythonic API and uses objects and exceptions.

The official API documentation can be found at `readthedocs.org`_

For more information on Libdiscid see `libdiscid`_.

For more information about the calculation of these disc ids see `Disc
ID Calculation`_.

Usage
~~~~~
::

    # this will load Libdiscid
    import discid

    disc = discid.read("/dev/cdrom")
    print "id: %s" % disc.id  # Python 2
    print("id: %s" % disc.id) # Python 3

See also the examples.py.

License
~~~~~~~
This module is released under the GNU Lesser General Public License
Version 3. See COPYING.LESSER for details.

Bugs
~~~~
You can submit tickets at `GitHub`_.

.. _readthedocs.org: https://python-discid.readthedocs.org/
.. _libdiscid: http://musicbrainz.org/doc/libdiscid
.. _Disc ID Calculation: http://musicbrainz.org/doc/Disc_ID_Calculation
.. _GitHub: https://github.com/JonnyJD/python-discid
