# Copyright (C) 2013  Johannes Dewender
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Please submit bug reports to GitHub:
# https://github.com/JonnyJD/python-discid/issues
"""Python binding of Libdiscid

Libdiscid is a library to calculate MusicBrainz Disc IDs.
This module provides a python-like API for that functionality.

The user is expected to create a :class:`Disc` object
using :func:`read` or :func:`put` and extract the generated information.

Importing this module will open libdiscid at the same time
and will raise :exc:`OSError` when libdiscid is not found.
"""

from discid.disc import read, put, Disc, DiscError, TOCError
from discid.track import Track
from discid.libdiscid import get_default_device
import discid.libdiscid
import discid.disc

__version__ = "1.1.0"


# these contants are defined here so sphinx can catch the "docstrings"

LIBDISCID_VERSION_STRING = discid.libdiscid.LIBDISCID_VERSION_STRING
"""The version string of the loaded libdiscid in the form `libdiscid x.y.z`.
For old versions the string is `libdiscid < 0.4.0`.
"""

FEATURES = discid.libdiscid.FEATURES
"""The features libdiscid supports for the platform as a list of strings.
Some Functions can raise :exc:`NotImplementedError` when a feature
is not available.
Some features might not be implemented in this python module,
see :data:`FEATURES_IMPLEMENTED`.
"""

FEATURES_IMPLEMENTED = discid.disc.FEATURES_IMPLEMENTED
"""The features implemented in this python module as a list of strings.
Some might not be available for your platform, see :data:`FEATURES`.
"""
