#!/usr/bin/env python3

import re
import sys
from collections import defaultdict
import ast
import copy
import json

def average(ys):
    return sum(ys) / len(ys)

class Parser:
    def __init__(self, strings=None, tags=None):
        if tags is None:
            tags = []
        if strings is None:
            strings = []
        self.strings = strings
        self.data = []
        self.tags = tags

    def load_from_file(self, filename):
        with open(filename, 'r') as infile:
            dict = json.load(infile)
            self.strings = dict["strings"]
            self.data = dict["data"]
            self.tags = dict["tags"]

    def save_to_file(self, filename):
        with open(filename, 'w') as outfile:
            dict = {
                "strings": self.strings,
                "data": self.data,
                "tags": self.tags
            }
            json.dump(dict, outfile)


    def get_typed_value_(self, value):
        try:
            typed_value = ast.literal_eval(value)
        except:
            typed_value = value
        return typed_value

    def add_input(self, fname):
        current_datum = []
        index = 0
        for line in open(fname, 'r'):
            string = self.strings[index]
            line = line.rstrip()
            m = re.match(string, line)
            if m:
                current_datum += [self.get_typed_value_(x) for x in m.groups()]
                index += 1
                print('Setting current tuple to %s based on "%s"' % (current_datum,line))
                if index == len(self.strings): # reach end
                    index = 0
                    self.data.append(current_datum)
                    current_datum = []

    def output_raw_data(self):
        return self.data

    def filter(self, fun):
        data = copy.deepcopy(self.data)
        self.data = []
        for datum in data:
            if fun(datum):
                self.data.append(datum)

    def add_entry(self, fun):
        for i, datum in enumerate(self.data):
            self.data[i].append(fun(self.data[i]))

    def render(self, idx, fun):
        for i, datum in enumerate(self.data):
            self.data[i][idx] = fun(datum[idx])

    # selector takes in a key, returns an integer key value or nothing
    # column is an integer index into the values
    def extract_lines(self, idx_x, idx_y, idx_keys=None, sort_key=lambda x: 0, relabel=lambda x: x, reduce_fun = average):
        if idx_keys is None:
            idx_keys = []
        raw_data = self.data

        raw_lines = defaultdict(list)

        for datum in raw_data:
            line_key = tuple(x for i, x in enumerate(datum) if i in idx_keys)
            line_data = (datum[idx_x], datum[idx_y])
            raw_lines[line_key].append(line_data)

        lines = []
        for label, line_data in raw_lines.items():
            average_data = self.reduce_data(line_data, reduce_fun)
            data = sorted(average_data)
            domain = [x[0] for x in data]
            lrange = [x[1] for x in data]
            lines.append({'label': label, 'domain': domain, 'range': lrange})

        sorted(lines, key=sort_key)

        for i, line in enumerate(lines):
            line["label"] = relabel(line["label"])

        return lines

    # def extract_pie(self, labels):

    def reduce_data(self, data, reduce_fun):
        helper = defaultdict(list)
        for x, y in data:
            helper[x].append(y)

        new_data = []
        for x, ys in helper.items():
            # Values, a list of tuples
            y = reduce_fun(ys)
            new_data.append((x, y))
        return new_data

    def reduce(self, idxs, reduce_fun=average):
        helper = defaultdict(list)
        for datum in self.data:
            key = [datum[idx] for idx in idxs]
            value = [datum[i] for i, x in enumerate(datum) if i not in idxs]
            helper[key].append(value)

        new_data = []
        for x, ys in helper.items():
            y = []
            for i in range(len(ys)):
                tt = reduce_fun([t[i] for t in ys])
                y.append(tt)
            new_data.append((x, y))
        self.data = new_data
