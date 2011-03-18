# smolt - Fedora hardware profiler
#
# Copyright (C) 2009 Sebastian Pipping <sebastian@pipping.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.

import portage
import distros.shared.html as html
from gate import Gate

try:
    set
except NameError:
    from sets import Set as set  # Python 2.3 fallback


class _Features:
    def __init__(self):
        self._fill_features()

    def _fill_features(self):
        def pre_evaluate_to_set(use_flags):
            # .. -foo .. foo .. --> foo
            # .. foo .. -foo .. --> -foo
            d = {}
            for i in [e.lstrip('+') for e in use_flags]:
                if i.startswith('-'):
                    enabled = False
                    flag = i.lstrip('-')
                else:
                    enabled = True
                    flag = i
                d[flag] = enabled
            return set((enabled and flag or '-' + flag) for flag, enabled in d.items())

        def get_features(section):
            res = pre_evaluate_to_set(portage.settings.configdict[section].get("FEATURES", "").split())
            return res

        publish_features = Gate().grants('gentoo', 'features')
        publish_system_profile = Gate().grants('gentoo', 'system_profile')


        self._features = {}
        for key in ('defaults', 'conf', 'env', 'globals'):
            self._features[key] = {}

        self._features['defaults']['publish'] = \
                publish_features and publish_system_profile
        self._features['conf']['publish'] = publish_features
        self._features['globals']['publish'] = publish_features
        self._features['env']['publish'] = \
                self._features['defaults']['publish'] and \
                self._features['globals']['publish'] and \
                self._features['conf']['publish']

        _all_features = {}
        for key in ('defaults', 'conf', 'env', 'globals'):
            if self._features[key]['publish']:
                _all_features[key] = get_features(key)
            else:
                _all_features[key] = []

        for key in ('defaults', 'conf', 'env', 'globals'):
            self._features[key]['entries'] = \
                    set(_all_features[key])

    def serialize(self):
        res = {
            'profile':sorted(self._features['defaults']['entries']),
            'make_conf':sorted(self._features['conf']['entries']),
            'make_globals':sorted(self._features['globals']['entries']),
            'final':sorted(self._features['env']['entries']),
        }
        return res

    def get_metrics(self, target_dict):
        fill_jobs = {
            'features_profile':'defaults',
            'features_make_conf':'conf',
            'features_make_globals':'globals',
            'features_final':'env',
        }
        for target, source in fill_jobs.items():
            target_dict[target] = (
                    self._features[source]['publish'], \
                    0, \
                    self._features[source]['publish'] and \
                        len(self._features[source]['entries']) or 0)

    def _dump_html_section(self, lines, title, data, line_break=True):
        lines.append('<h3>%s</h3>' % html.escape(title))
        lines.append('<ul>')
        for i in sorted(data):
            lines.append('<li>%s</li>' % html.escape(i))
        lines.append('</ul>')

    def dump_html(self, lines):
        serialized = self.serialize()
        lines.append('<h2>Features</h2>')
        self._dump_html_section(lines, 'From make.conf', serialized['make_conf'])
        self._dump_html_section(lines, 'From make.globals', serialized['make_globals'])
        self._dump_html_section(lines, 'From system profile', serialized['profile'])
        self._dump_html_section(lines, 'Combined', serialized['final'], line_break=False)

    def _dump_rst_section(self, lines, title, data, line_break=True):
        lines.append(title)
        lines.append('`````````````````````')
        for i in sorted(data):
            lines.append('- %s' % i)
        if line_break:
            lines.append('')

    def dump_rst(self, lines):
        serialized = self.serialize()
        lines.append('Features')
        lines.append('-----------------------------')
        self._dump_rst_section(lines, 'From make.conf', serialized['make_conf'])
        self._dump_rst_section(lines, 'From make.globals', serialized['make_globals'])
        self._dump_rst_section(lines, 'From system profile', serialized['profile'])
        self._dump_rst_section(lines, 'Combined', serialized['final'], line_break=False)

    def _dump(self):
        lines = []
        self.dump_rst(lines)
        print '\n'.join(lines)
        print


_features_instance = None
def Features():
    """
    Simple singleton wrapper around _Features class
    """
    global _features_instance
    if _features_instance == None:
        _features_instance = _Features()
    return _features_instance


if __name__ == '__main__':
    features = Features()
    features._dump()
