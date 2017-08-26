# -*- coding: utf-8 -*-

'''
Patches tvdb specific create_key in requests_cache which only includes
Accept-Language in the key

This module must be imported before any modules use requests_cache
explicitly or implicitly
'''

import requests_cache

import hashlib
def create_key(self, request):
    try:
        if self._ignored_parameters:
            url, body = self._remove_ignored_parameters(request)
        else:
            url, body = request.url, request.body
    except AttributeError:
        url, body = request.url, request.body
    key = hashlib.sha256()
    key.update(requests_cache.backends.base._to_bytes(request.method.upper()))
    key.update(requests_cache.backends.base._to_bytes(url))
    if request.body:
        key.update(requests_cache.backends.base._to_bytes(body))
    else:
        if self._include_get_headers and request.headers != requests_cache.backends.base._DEFAULT_HEADERS:
            for name, value in sorted(request.headers.items()):
                # include only Accept-Language as it is important for context
                if name in ['Accept-Language']:
                    key.update(requests_cache.backends.base._to_bytes(name))
                    key.update(requests_cache.backends.base._to_bytes(value))
    return key.hexdigest()

requests_cache.backends.base.BaseCache.create_key = create_key
