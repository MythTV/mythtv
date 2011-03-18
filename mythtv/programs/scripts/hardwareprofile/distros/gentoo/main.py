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

import os
from distros.distro import Distro
import distros.shared.html as html


_PRIVACY_METRICS_COLUMN_HEADERS = ('Data class', 'Publish', \
        'Count private', 'Count non-private')

_DATA_CLASS_LABEL_MAP = {
    'arch_related':'Architecture-related',
    'call_flags_cflags':'Compile flags (CFLAGS)',
    'call_flags_cxxflags':'Compile flags (CXXFLAGS)',
    'call_flags_ldflags':'Compile flags (LDFLAGS)',
    'call_flags_makeopts':'Compile flags (MAKEOPTS)',
    'features_profile':'Features (system profile)',
    'features_make_conf':'Features (make.conf)',
    'features_make_globals':'Features (make.globals)',
    'features_final':'Features (combined)',
    'global_use_flags_make_conf':'Global use flags (make.conf)',
    'global_use_flags_profile':'Global use flags (system profile)',
    'global_use_flags_final':'Global use flags (combined)',
    'installed_packages':'Installed packages: General',
    'installed_packages_use_flags':'Installed packages: Use flags',
    'mirrors_distfiles':'Mirrors: Distfiles',
    'mirrors_sync':'Mirrors: Package tree',
    'package_mask':'Package mask entries',
    'repos':'Repositories',
    'system_profile':'System profile',
}

def _data_class_key_to_label(key):
    try:
        return _DATA_CLASS_LABEL_MAP[key]
    except KeyError:
        return key


class _Gentoo(Distro):
    def key(self):
        return 'gentoo'

    def detected(self, debug=False):
        """
        Returns True if we run on top of Gentoo, else False.
        """
        return os.path.exists('/etc/gentoo-release')

    def gather(self, debug=False):
        def _stage(text):
            print 'Processing %s' % (text)
        # Local imports to not pull missing dependencies in
        # on non-Gentoo machines.
        from globaluseflags import GlobalUseFlags
        from compileflags import CompileFlags
        from mirrors import Mirrors
        from overlays import Overlays
        from packagestar import PackageMask
        from systemprofile import SystemProfile
        from trivials import Trivials
        from features import Features
        from installedpackages import InstalledPackages

        _stage('global use flags')
        global_use_flags = GlobalUseFlags()

        _stage('compile flags')
        compile_flags = CompileFlags()

        _stage('mirrors')
        mirrors = Mirrors(debug=debug)

        _stage('overlays')
        overlays = Overlays()

        _stage('package.mask entries')
        user_package_mask = PackageMask()

        _stage('features')
        features = Features()

        _stage('trivials')
        trivials = Trivials()

        _stage('installed packages (takes some time)')
        if debug:
            def cb_enter(cpv, i, count):
                print '[% 3d%%] %s' % (i * 100 / count, cpv)
        else:
            def cb_enter(*_):
                pass
        installed_packages = InstalledPackages(debug=debug, cb_enter=cb_enter)

        machine_data = {}
        html_lines = []
        rst_lines = []
        metrics_dict = {}

        html_lines.append('<h1>Gentoo</h1>')
        rst_lines.append('Gentoo')
        rst_lines.append('=================================')
        rst_lines.append('')
        machine_data['protocol'] = '1.2'

        trivials.dump_html(html_lines)
        trivials.dump_rst(rst_lines)
        rst_lines.append('')
        trivials.get_metrics(metrics_dict)

        machine_data['features'] = features.serialize()
        features.dump_html(html_lines)
        features.dump_rst(rst_lines)
        rst_lines.append('')
        features.get_metrics(metrics_dict)

        machine_data['call_flags'] = compile_flags.serialize()
        compile_flags.dump_html(html_lines)
        compile_flags.dump_rst(rst_lines)
        rst_lines.append('')
        compile_flags.get_metrics(metrics_dict)

        machine_data['mirrors'] = mirrors.serialize()
        mirrors.dump_html(html_lines)
        mirrors.dump_rst(rst_lines)
        rst_lines.append('')
        mirrors.get_metrics(metrics_dict)

        machine_data['repos'] = overlays.serialize()
        overlays.dump_html(html_lines)
        overlays.dump_rst(rst_lines)
        rst_lines.append('')
        overlays.get_metrics(metrics_dict)

        machine_data['user_package_mask'] = user_package_mask.serialize()
        user_package_mask.dump_html(html_lines)
        user_package_mask.dump_rst(rst_lines)
        rst_lines.append('')
        user_package_mask.get_metrics(metrics_dict)

        machine_data['global_use_flags'] = global_use_flags.serialize()
        global_use_flags.dump_html(html_lines)
        global_use_flags.dump_rst(rst_lines)
        rst_lines.append('')
        global_use_flags.get_metrics(metrics_dict)

        machine_data['installed_packages'] = installed_packages.serialize()
        installed_packages.dump_html(html_lines)
        installed_packages.dump_rst(rst_lines)
        installed_packages.get_metrics(metrics_dict)

        for container in (trivials, ):
            for k, v in container.serialize().items():
                key = k.lower()
                if key in machine_data:
                    raise Exception('Unintended key collision')
                machine_data[key] = v

        machine_data['privacy_metrics'] = metrics_dict
        self.dump_metrics_html(html_lines, metrics_dict)
        rst_lines.append('')
        self.dump_metrics_rst(rst_lines, metrics_dict)

        excerpt_lines = []
        excerpt_lines.append('ACCEPT_KEYWORDS: ' + ' '.join(trivials.serialize()['accept_keywords']))
        excerpt_lines.append('CXXFLAGS: ' + ' '.join(compile_flags.serialize()['cxxflags']))
        excerpt_lines.append('MAKEOPTS: ' + ' '.join(compile_flags.serialize()['makeopts']))
        excerpt_lines.append('...')

        self._data = machine_data
        self._html = '\n'.join(html_lines)
        self._rst = '\n'.join(rst_lines)
        self._excerpt = '\n'.join(excerpt_lines)

    def dump_metrics_html(self, lines, metrics_dict):
        lines.append('<h2>Metrics</h2>')
        lines.append('Note: This meta data is also included with the submission.')

        lines.append('<table border="1" cellspacing="1" cellpadding="4">')
        lines.append('<tr>')
        for i in _PRIVACY_METRICS_COLUMN_HEADERS:
            lines.append('<th>%s</th>' % html.escape(i))
        lines.append('</tr>')
        for k, v in sorted(metrics_dict.items()):
            lines.append('<tr>')
            published, count_private, count_non_private = v
            for i in (_data_class_key_to_label(k), ):
                lines.append('<td>%s</td>' % html.escape(i))
            for i in (published, ):
                lines.append('<td align="center">%s</td>' % (i and 'yes' or 'no'))
            for i in (count_private, count_non_private):
                lines.append('<td align="right">%s</td>' % html.escape(str(i)))
            lines.append('</tr>')
        lines.append('</table>')

        lines.append('<br>')
        lines.append('<br>')
        lines.append('Legend:')
        lines.append('<ul>')
        lines.append('<li>Data class: Label of the class of data</li>')
        lines.append('<li>Publish: Flag if data of this class will be submitted</li>')
        lines.append('<li>Count private: Number of entries not to be submitted</li>')
        lines.append('<li>Count non-private: Number of entries to be submitted</li>')
        lines.append('</ul>')

    def dump_metrics_rst(self, lines, metrics_dict):
        max_length = [2, 2, 2, 2]
        def final_values(column_values):
            res = list(column_values)
            res[0] = res[0] + ' '
            res[1] = res[1] and 'yes' or 'no'
            return tuple(res)

        def measure_line(column_values):
            for i, v in enumerate(column_values):
                cur_len = 2 + len(str(v))
                if cur_len > max_length[i]:
                    max_length[i] = cur_len
        def put_seperator(lines):
            lines.append('+%s+' % '+'.join('-'*e for e in max_length))

        def put_line(lines, column_values):
            lines.append('|%s|' % '|'.join(\
                (i >= 2) and \
                    (' '*(max_length[i] - len(str(v)) - 1) + str(v)) + ' ' \
                or \
                    (' ' + (str(v) + ' '*(max_length[i] - len(str(v)) - 1))) \
                for i, v in enumerate(column_values)))

        lines.append('Metrics')
        lines.append('-----------------------------')
        lines.append('Note: This meta data is also included with the submission.')
        measure_line(_PRIVACY_METRICS_COLUMN_HEADERS)
        for k, v in metrics_dict.items():
            published, count_private, count_non_private = v
            measure_line(final_values((_data_class_key_to_label(k), ) + v))
        put_seperator(lines)
        put_line(lines, _PRIVACY_METRICS_COLUMN_HEADERS)
        for k, v in sorted(metrics_dict.items()):
            published, count_private, count_non_private = v
            put_seperator(lines)
            put_line(lines, final_values((_data_class_key_to_label(k), published, count_private, count_non_private)))
        put_seperator(lines)
        lines.append('')
        lines.append('Legend:')
        lines.append('- Data class: Label of the class of data')
        lines.append('- Publish: Flag if data of this class will be submitted')
        lines.append('- Count private: Number of entries not to be submitted')
        lines.append('- Count non-private: Number of entries to be submitted')

    def data(self):
        return self._data

    def html(self):
        return self._html

    def rst(self):
        return self._rst

    def rst_excerpt(self):
        return self._excerpt


_gentoo_instance = None
def Gentoo():
    """
    Simple singleton wrapper around _Gentoo class
    """
    global _gentoo_instance
    if _gentoo_instance == None:
        _gentoo_instance = _Gentoo()
    return _gentoo_instance


if __name__ == '__main__':
    # Enable auto-flushing for stdout
    import sys
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

    Gentoo().gather(debug=True)
    """
    from simplejson import JSONEncoder
    print JSONEncoder(indent=2, sort_keys=True).encode(
        Gentoo().data())
    """
    print Gentoo().rst()
