#!/usr/bin/env python
# -*- coding: utf-8 -*-
from string import ascii_uppercase, ascii_lowercase

class getData(object):
    def __init__(self):
        self.a = '3530346e36313539393971733336717171323031323134377036356f6f367030'
        self.s = '3330373736326f7236716e30323171'

    def update(self, data):
        total = []
        for char in data.decode("hex"):
            if char in ascii_uppercase:
                index = (ascii_uppercase.find(char) + 13) % 26
                total.append(ascii_uppercase[index])
            elif char in ascii_lowercase:
                index = (ascii_lowercase.find(char) + 13) % 26
                total.append(ascii_lowercase[index])
            else:
                total.append(char)
        return "".join(total)
