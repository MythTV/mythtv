# -*- coding: utf-8 -*-

from string import ascii_uppercase, ascii_lowercase

class getData(object):
    def __init__(self):
        self.a = b'3530346e36313539393971733336717171323031323134377036356f6f367030'
        self.s = b'3330373736326f7236716e30323171'

    def update_old(self, data):
        total = []
        for char in bytes.fromhex(data).decode('ascii'):
            if char in ascii_uppercase:
                index = (ascii_uppercase.find(char) + 13) % 26
                total.append(ascii_uppercase[index])
            elif char in ascii_lowercase:
                index = (ascii_lowercase.find(char) + 13) % 26
                total.append(ascii_lowercase[index])
            else:
                total.append(char)
        return "".join(total)

    def update(self, data):
        total = []
        for char in bytes.fromhex(data).decode('ascii'):
            print(char)
