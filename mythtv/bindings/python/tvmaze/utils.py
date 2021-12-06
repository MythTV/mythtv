# -*- coding: UTF-8 -*-

from datetime import datetime
import sys

from io import StringIO
from html.parser import HTMLParser


class MLStripper(HTMLParser):
    def __init__(self):
        super().__init__()
        self.reset()
        self.strict = False
        self.convert_charrefs= True
        self.text = StringIO()

    def handle_data(self, d):
        self.text.write(d)

    def get_data(self):
        return self.text.getvalue()


def strip_tags(html):
    if html is not None and html != "":
        s = MLStripper()
        s.feed(html)
        return s.get_data()
    else:
        return ""


def convert_date(tstring):
    if tstring is None or tstring == '':
        return None
    try:
        return datetime.strptime(tstring, '%Y-%m-%d').date()
    except(TypeError, ValueError):
        return None


def convert_time(tstring):
    if tstring is None or tstring == '':
        return None
    try:
        return datetime.strptime(tstring, '%Y-%m-%d')
    except(TypeError, ValueError):
        return None
