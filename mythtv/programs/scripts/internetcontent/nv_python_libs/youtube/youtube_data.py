#!/usr/bin/env python
# -*- coding: utf-8 -*-
from string import ascii_uppercase, ascii_lowercase

class getData(object):
    def __init__(self):
        self.a = '4e566d6e466c50784b416e477a527176587337444b5730554530324e4d39706d6774535531614c'

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
