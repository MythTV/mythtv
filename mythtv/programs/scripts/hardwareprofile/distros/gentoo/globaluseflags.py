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
import re
import os
from tools.maintreedir import main_tree_dir

import os
import sys
import distros.shared.html as html
from gate import Gate

try:
    set
except NameError:
    from sets import Set as set  # Python 2.3 fallback


def _flatten_dict_tree(dict_tree):
    def process_compressables(compressables, res):
        if len(compressables) > 1:
            res.append((k + '_' + '{' + ','.join(s for (s, d) in compressables) + '}', 1))
        else:
            res.append((k + '_' + compressables[0][0], 1))

    res = []
    for k in sorted(dict_tree.keys()):
        flat = _flatten_dict_tree(dict_tree[k])
        if not flat:
            res.append((k, 0))
        else:
            if len(flat) > 1:
                compressables = []
                for (s, d) in sorted(flat):
                    if d > 0:
                        if compressables:
                            process_compressables(compressables, res)
                            compressables = []
                        res.append((k + '_' + s, d))
                    else:
                        compressables.append((s, d))
                if compressables:
                    process_compressables(compressables, res)
            else:
                res.append((k + '_' + flat[0][0], flat[0][1]))
    return res

def compress_use_flags(flag_list):
    # Convert to tree
    dict_tree = {}
    for f in flag_list:
        parts = f.split('_')
        d = dict_tree
        for i, v in enumerate(parts):
            if v not in d:
                d[v] = {}
            d = d[v]

    # Flatten back
    return [s for (s, d) in _flatten_dict_tree(dict_tree)]


class _GlobalUseFlags:
    def __init__(self):
        self._fill_use_flags()

    def _registered_global_use_flags(self):
        global_line_sub = re.compile('^([^ ]+) - .*\\n')
        global_line_filter = re.compile('^[^ ]+ - ')
        try:
            f = open(os.path.join(main_tree_dir(), 'profiles', 'use.desc'), 'r')
            lines = [global_line_sub.sub('\\1', l) for l in
                    f.readlines() if global_line_filter.match(l)]
            f.close()
            return set(lines)
        except IOError:
            return set()

    def _registered_local_use_flags(self):
        local_line_sub = re.compile('^[^ :]+:([^ ]+) - .*\\n')
        local_line_filter = re.compile('^[^ :]+:[^ ]+ - ')
        try:
            f = open(os.path.join(main_tree_dir(), 'profiles',
                'use.local.desc'), 'r')
            lines = [local_line_sub.sub('\\1', l) for l in
                    f.readlines() if local_line_filter.match(l)]
            f.close()
            return set(lines)
        except IOError:
            return set()

    def _expanded_use_flags(self):
        use_flags = []
        expand_desc_dir = os.path.join(main_tree_dir(), 'profiles', 'desc')
        try:
            expand_list = os.listdir(expand_desc_dir)
        except OSError:
            pass
        else:
            for desc_filename in expand_list:
                if not desc_filename.endswith('.desc'):
                    continue
                use_prefix = desc_filename[:-5].lower() + '_'
                for line in portage.grabfile(os.path.join(
                        expand_desc_dir, desc_filename)):
                    x = line.split()
                    if x:
                        use_flags.append(use_prefix + x[0])
        return set(use_flags)

    def _auto_use_flags(self):
        return set(portage.grabfile(os.path.join(main_tree_dir(), 'profiles',
            'arch.list')))

    def _fill_use_flags(self):
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

        def get_use_flags(section):
            res = pre_evaluate_to_set(portage.settings.configdict[section].get("USE", "").split())
            use_expand = portage.settings.configdict[section].get("USE_EXPAND", "").split()
            for expand_var in use_expand:
                expand_var_values = portage.settings.configdict[section].get(expand_var, "").split()
                prefix = expand_var.lower()
                res = res.union(set('%s_%s' % (prefix, e) for e in expand_var_values))
            return res

        publish_global_use_flags = Gate().grants('gentoo', 'global_use_flags')
        publish_system_profile = Gate().grants('gentoo', 'system_profile')


        self._use_flags = {}
        for key in ('defaults', 'conf', 'env'):
            self._use_flags[key] = {}

        self._use_flags['defaults']['publish'] = \
                publish_global_use_flags and publish_system_profile
        self._use_flags['conf']['publish'] = publish_global_use_flags
        self._use_flags['env']['publish'] = \
                self._use_flags['defaults']['publish'] and \
                self._use_flags['conf']['publish']

        _all_use_flags = {}
        for key in ('defaults', 'conf', 'env'):
            if self._use_flags[key]['publish']:
                _all_use_flags[key] = get_use_flags(key)
            else:
                _all_use_flags[key] = []

        # Filter our private use flags
        self._non_private_space = self._registered_global_use_flags().union(
                self._registered_local_use_flags()).union(
                self._expanded_use_flags()).union(
                self._auto_use_flags())

        def is_non_private(x):
            positive = x.lstrip('-')
            return positive in self._non_private_space

        for key in ('defaults', 'conf', 'env'):
            self._use_flags[key]['flags'] = \
                    set(e for e in _all_use_flags[key] if is_non_private(e))
            self._use_flags[key]['count_non_private'] = \
                    len(self._use_flags[key]['flags'])
            self._use_flags[key]['count_private'] = len(_all_use_flags[key]) - \
                    self._use_flags[key]['count_non_private']

    def is_known(self, flag):
        return flag in self._non_private_space

    def serialize(self):
        res = {
            'profile':sorted(self._use_flags['defaults']['flags']),
            'make_conf':sorted(self._use_flags['conf']['flags']),
            'final':sorted(self._use_flags['env']['flags']),
        }
        return res

    def get_metrics(self, target_dict):
        fill_jobs = {
            'global_use_flags_profile':'defaults',
            'global_use_flags_make_conf':'conf',
            'global_use_flags_final':'env',
        }
        for target, source in fill_jobs.items():
            target_dict[target] = (
                    self._use_flags[source]['publish'], \
                    self._use_flags[source]['count_private'], \
                    self._use_flags[source]['count_non_private'])

    def dump_html(self, lines):
        serialized = self.serialize()
        lines.append('<h2>Global use flags</h2>')
        lines.append('<h3>From make.conf</h3>')
        lines.append('<p>')
        lines.append(html.escape(', '.join(compress_use_flags(serialized['make_conf']))))
        lines.append('</p>')
        lines.append('<h3>From system profile</h3>')
        lines.append('<p>')
        lines.append(html.escape(', '.join(compress_use_flags(serialized['profile']))))
        lines.append('</p>')
        lines.append('<h3>Combined</h3>')
        lines.append('<p>')
        lines.append(html.escape(', '.join(compress_use_flags(serialized['final']))))
        lines.append('</p>')

    def dump_rst(self, lines):
        serialized = self.serialize()
        lines.append('Global use flags')
        lines.append('-----------------------------')
        lines.append('From make.conf')
        lines.append('`````````````````````')
        lines.append('  '.join(compress_use_flags(serialized['make_conf'])))
        lines.append('')
        lines.append('From system profile')
        lines.append('`````````````````````')
        lines.append('  '.join(compress_use_flags(serialized['profile'])))
        lines.append('')
        lines.append('Combined')
        lines.append('`````````````````````')
        lines.append('  '.join(compress_use_flags(serialized['final'])))

    def _dump(self):
        lines = []
        self.dump_rst(lines)
        print '\n'.join(lines)
        print


_global_use_flags_instance = None
def GlobalUseFlags():
    """
    Simple singleton wrapper around _GlobalUseFlags class
    """
    global _global_use_flags_instance
    if _global_use_flags_instance == None:
        _global_use_flags_instance = _GlobalUseFlags()
    return _global_use_flags_instance


if __name__ == '__main__':
    global_use_flags = GlobalUseFlags()
    global_use_flags._dump()
