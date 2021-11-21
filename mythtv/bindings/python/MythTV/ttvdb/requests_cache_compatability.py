# -*- coding: utf-8 -*-

'''
Patches older versions of requests_cache with missing expire
functionality and an updated create_key which excludes most
HEADERS

This module must be imported before any modules use requests_cache
explicitly or implicitly
'''

import requests_cache
from datetime import datetime, timedelta

rc_version_info = tuple(int(x) for x in requests_cache.__version__.split('.'))
# patch if required if older versions than '0.5.0'
# request_cache.core is deprecated in '0.6.0' and removed on '0.8.0'.
if rc_version_info < (0, 5, 0):
    try:
        requests_cache.backends.base.BaseCache.remove_old_entries
    except Exception as e:
        def remove_old_entries(self, created_before):
            """ Deletes entries from cache with creation time older than ``created_before``
            """
            keys_to_delete = set()
            for key in self.responses:
                try:
                    response, created_at = self.responses[key]
                except KeyError:
                    continue
                if created_at < created_before:
                    keys_to_delete.add(key)

            for key in keys_to_delete:
                self.delete(key)


        def remove_expired_responses(self):
            """ Removes expired responses from storage
            """
            if not self._cache_expire_after:
                return
            # just in case expire_after is not converted in the
            #  original constructor, convert it to a timedelta
            if not isinstance(self._cache_expire_after, timedelta):
                self._cache_expire_after = timedelta(seconds=self._cache_expire_after)

            self.cache.remove_old_entries(datetime.utcnow() - self._cache_expire_after)


        requests_cache.backends.base.BaseCache.remove_old_entries = remove_old_entries
        requests_cache.core.CachedSession.remove_expired_responses = remove_expired_responses
