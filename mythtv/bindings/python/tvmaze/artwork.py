# -*- coding: UTF-8 -*-

#  Copyright (c) 2021 Steve Erlenborn
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.


class Artwork(object):
    def __init__(self, data):
        self.id = data.get('id')        # ID for the artwork
        self.type = data.get('type')    # 'poster', 'background', or 'banner'
        self.main = data.get('main')    # 'true' for official artwork, released by the Network

        if (data.get('resolutions') is not None) and \
           (data.get('resolutions').get('original') is not None):
            self.original = data.get('resolutions').get('original').get('url')
        else:
            self.original = None

        if (data.get('resolutions') is not None) and \
           (data.get('resolutions').get('medium') is not None):
            self.medium = data.get('resolutions').get('medium').get('url')
        else:
            self.medium = None

    def __str__(self):
        return self.original
