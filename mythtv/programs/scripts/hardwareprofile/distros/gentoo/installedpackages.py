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
from portage.dbapi.vartree import vartree
from portage.versions import catpkgsplit
from overlays import Overlays
from globaluseflags import GlobalUseFlags, compress_use_flags
from worldset import WorldSet
from packagestar import PackageMask, PackageUnmask, ProfilePackageMask
from packageprivacy import is_private_package_atom

import os
import sys
import distros.shared.html as html
from gate import Gate

class InstalledPackages:
    def __init__(self, debug=False,
            cb_enter=None, cb_done=None):
        self._publish_installed_packages = Gate().grants('gentoo', 'installed_packages')
        self._publish_installed_packages_use_flags = Gate().grants('gentoo', 'installed_packages_use_flags')
        self._publish_repos = Gate().grants('gentoo', 'repositories')

        self._cpv_flag_list = []
        self._private_count = 0
        self._private_use_flags = 0
        self._non_private_use_flags = 0
        if self._publish_installed_packages:
            var_tree = vartree()
            installed_cpvs = var_tree.getallcpv()  # TODO upstream plans rename?
            self._total_count = len(installed_cpvs)
            i = 0
            for cpv in sorted(installed_cpvs):
                i = i + 1
                if cb_enter:
                    cb_enter(cpv, i, self._total_count)
                entry = self._process(var_tree, cpv, debug=debug)
                if entry:
                    self._cpv_flag_list.append(entry)
                else:
                    self._private_count = self._private_count + 1
        else:
            self._total_count = 0
        if cb_done:
            cb_done()

    def _keyword_status(self, ARCH, ACCEPT_KEYWORDS, KEYWORDS):
        k = set(KEYWORDS.split(' '))
        if ARCH in k:
            return ''
        TILDE_ARCH = '~' + ARCH
        if TILDE_ARCH in k:
            ak = set(ACCEPT_KEYWORDS.split(' '))
            if TILDE_ARCH in ak:
                return ''
            else:
                return '~arch'
        else:
            return '**'

    def is_known_use_flag(self, flag):
        known = GlobalUseFlags().is_known(flag)
        if not known:
            print '  skipping private use flag "%s"' % flag
        return known

    def _process(self, var_tree, cpv, debug=False):
        cat, pkg, ver, rev = catpkgsplit(cpv)
        package_name = "%s/%s" % (cat, pkg)
        if rev == 'r0':
            version_revision = ver
        else:
            version_revision = "%s-%s" % (ver, rev)

        SLOT, KEYWORDS, repo, IUSE, USE = \
            var_tree.dbapi.aux_get(cpv, ['SLOT', 'KEYWORDS', 'repository',
            'IUSE', 'USE'])

        # Perform privacy check and filtering
        installed_from = [repo, ]
        if is_private_package_atom('=' + cpv, installed_from=installed_from,
                debug=debug):
            return None
        repo = installed_from[0]

        ACCEPT_KEYWORDS = portage.settings['ACCEPT_KEYWORDS']
        ARCH = portage.settings['ARCH']
        keyword_status = self._keyword_status(ARCH, ACCEPT_KEYWORDS, KEYWORDS)

        unmasked = PackageUnmask().hits(cpv)
        # A package that is (1) installed and (2) not unmasked
        # cannot be masked so we skip the next line's checks
        masked = unmasked and (PackageMask().hits(cpv) or \
            ProfilePackageMask().hits(cpv))

        # World set test
        if SLOT != '0':
            world_set_test = '%s/%s:%s' % (cat, pkg, SLOT)
        else:
            world_set_test = '%s/%s' % (cat, pkg)
        is_in_world = world_set_test in WorldSet().get()

        # Use flags
        if self._publish_installed_packages_use_flags:
            iuse_list = tuple(x.lstrip("+-") for x in IUSE.split())
            count_all = len(set(iuse_list))

            package_use_set = set(filter(self.is_known_use_flag, USE.split()))
            package_iuse_set = set(filter(self.is_known_use_flag, iuse_list))
            enabled_flags = package_use_set & package_iuse_set
            disabled_flags = package_iuse_set - package_use_set
            package_flags = sorted(enabled_flags) + ['-' + e for e in sorted(disabled_flags)]

            count_non_private = len(package_flags)
            count_private = count_all - count_non_private

            self._private_use_flags = self._private_use_flags + count_private
            self._non_private_use_flags = self._non_private_use_flags + count_non_private
        else:
            package_flags = tuple()

        if not self._publish_repos:
            repo = 'WITHHELD'

        entry = [package_name, version_revision, SLOT, keyword_status,
            masked, unmasked, is_in_world, repo, package_flags]
        return entry

    def total_count(self):
        return self._total_count

    def private_count(self):
        return self._private_count

    def known_count(self):
        return len(self._cpv_flag_list)

    def serialize(self):
        return self._cpv_flag_list

    def get_metrics(self, target_dict):
        if self._publish_installed_packages:
            target_dict['installed_packages'] = (True, \
                    self._private_count, \
                    self._total_count - self._private_count)
        else:
            target_dict['installed_packages'] = (False, 0, 0)

        target_dict['installed_packages_use_flags'] = \
                (self._publish_installed_packages_use_flags, \
                self._private_use_flags, \
                self._non_private_use_flags)

    def dump_html(self, lines):
        def fix_empty(text):
            if text == '':
                return '&nbsp;'
            else:
                return text

        lines.append('<h2>Installed packages</h2>')
        lines.append('<table border="1" cellspacing="1" cellpadding="4">')
        lines.append('<tr>')
        for i in ('Package', 'Version', 'Slot', 'Keyword', 'Masked', 'Unmasked', 'World', 'Tree', 'Use flags'):
            lines.append('<th>%s</th>' % i)
        lines.append('</tr>')
        for list in self._cpv_flag_list:
            package_name, version_revision, SLOT, keyword_status, \
                masked, unmasked, is_in_world, repo, sorted_flags_list = \
                list

            lines.append('<tr>')
            for i in (package_name, version_revision):
                lines.append('<td>%s</td>' % html.escape(i))
            for i in (SLOT, ):
                if i == '0':  # Hide default slot
                    v = '&nbsp;'
                else:
                    v = html.escape(i)
                lines.append('<td>%s</td>' % v)
            for i in (keyword_status, ):
                lines.append('<td>%s</td>' % fix_empty(html.escape(i)))
            for i in (masked, unmasked, is_in_world):
                v = i and 'X' or '&nbsp;'  # Hide False
                lines.append('<td>%s</td>' % v)
            for i in (repo, ):
                lines.append('<td>%s</td>' % fix_empty(html.escape(i)))
            final_flag_list = [x.startswith('-') and \
                    '<s>%s</s>' % html.escape(x.lstrip('-')) or \
                    html.escape(x) for x in \
                    compress_use_flags(sorted_flags_list)]
            lines.append('<td>%s</td>' % fix_empty(', '.join(final_flag_list)))
            lines.append('</tr>')
        lines.append('</table>')

    def dump_rst(self, lines):
        lines.append('Installed packages')
        lines.append('-----------------------------')
        for list in self._cpv_flag_list:
            package_name, version_revision, SLOT, keyword_status, \
                masked, unmasked, is_in_world, repo, sorted_flags_list = \
                list

            lines.append('- %s-%s' % (package_name, version_revision))

            if SLOT != '0':  # Hide default slot
                lines.append('  - Slot: %s' % (SLOT))
            if keyword_status:
                lines.append('  - Keyword status: %s' % keyword_status)

            tag_names = ('masked', 'unmasked', 'world')
            values = (masked, unmasked, is_in_world)
            tags = []
            for i, v in enumerate(values):
                if v:
                    tags.append(tag_names[i])
            if tags:
                lines.append('  - Tags: %s' % ', '.join(tags))

            if repo:
                lines.append('  - Repository: %s' % (repo))
            if sorted_flags_list:
                final_flag_list = [x.startswith('-') and x or ('+' + x) for x in \
                        compress_use_flags(sorted_flags_list)]
                lines.append('  - Use flags: %s' % ', '.join(final_flag_list))

    def _dump(self):
        lines = []
        self.dump_rst(lines)
        print '\n'.join(lines)
        print

    """
    def dump(self):
        print 'Installed packages:'
        for list in self._cpv_flag_list:
            package_name, version_revision, SLOT, keyword_status, \
                masked, unmasked, is_in_world, repository, sorted_flags_list = \
                list
            tags = [e for e in [
                masked and 'MASKED' or '',
                unmasked and 'UNMASKED' or '',
                is_in_world and 'WORLD' or ''] if e]
            print [package_name, version_revision, SLOT, keyword_status,
                tags, repository, sorted_flags_list]
        print
        print '  Total: ' + str(self.total_count())
        print '    Known: ' + str(self.known_count())
        print '    Private: ' + str(self.private_count())
        print
    """

if __name__ == '__main__':
    def cb_enter(cpv, i, count):
        print '[%s/%s] %s' % (i, count, cpv)

    installed_packages = InstalledPackages(debug=True, cb_enter=cb_enter)
    installed_packages._dump()
