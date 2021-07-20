import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import sys
sys.path.append('../../include')
import plotexp

pd.set_option('display.float_format',lambda x : '%.3f' % x)

data = None
# columns = ['core_num', 'task', 'total_time']
tag_map = {
    "kcount-upcxx": "UPC++",
    "kcount-v0.3.1": "ARL v0.3.1",
    "kcount-v0.3.0": "ARL v0.3.0",
    "kcount-v0.2.0": "ARL v0.2.0",
    "kcount-v0.1.0": "ARL v0.1.0",
    "kcount_ffrd": "ARL rpc_ffrd",
    "kcount_ff": "ARL rpc_ff",
    "kcount_aggrd": "ARL rpc_aggrd",
    "kcount_agg": "ARL rpc_agg",
}
def draw_version():
    title = "Kmer Counting Benchmark"
    x_key = "core_num"
    tag_key = "task"
    y_key = 'total_time'
    # tag_mask = ["kcount-upcxx", "kcount_ffrd"]
    # tag_mask = ["kcount-upcxx", "kcount_ffrd", "kcount_ff", "kcount_aggrd", "kcount_agg"]
    tag_mask = ["kcount_ffrd", "kcount-v0.3.1", "kcount-v0.3.0", "kcount-v0.2.0", "kcount-v0.1.0", "kcount-upcxx"]

    df = pd.read_csv("kcount_200704.csv")[[tag_key, x_key, y_key]]
    lines = []

    for label in tag_mask:
        df1 = df[df[tag_key] == label]
        current_domain = []
        current_value = []
        for x in df1[x_key].unique():
            y = df1[df[x_key] == x].min()[y_key]
            if y is np.nan:
                continue
            if y == 0:
                continue
            current_domain.append(x)
            current_value.append(y)
        # current_domain, current_value = zip(*sorted(zip(current_domain, current_value)))
        # lines.append({'label': "{}={}".format(tag_key, label), 'domain': current_domain, 'range': current_value})
        if label in tag_map:
            label = tag_map[label]
        if label == "ARL rpc_ffrd":
            label = "ARL v0.4.0"
        lines.append({'label': label, 'domain': current_domain, 'range': current_value})

    print(lines)
    # plotexp.line_plot(title, x_key, y_key, lines, 'line1.png', False, is_show=False)
    # plotexp.line_plot(title, "core number", "total time (s)", lines, 'kcount_primitive.png', False, is_show=False)
    plotexp.line_plot(title, "core number", "total time (s)", lines, 'kcount_version.png', False, is_show=False)

def draw_primitive():
    title = "Kmer Counting Benchmark"
    x_key = "core_num"
    tag_key = "task"
    y_key = 'total_time'
    # tag_mask = ["kcount-upcxx", "kcount_ffrd"]
    tag_mask = [
        "kcount_ffrd",
        "kcount_ff",
        "kcount_aggrd",
        "kcount_agg",
        "kcount_upcxx",
        "kcount_ffrd aggBuffer=simple",
        "kcount_ffrd aggBuffer=local",
    ]

    df = pd.read_csv("data/kcount.csv")[[tag_key, x_key, y_key]]
    lines = []

    for label in tag_mask:
        df1 = df[df[tag_key] == label]
        current_domain = []
        current_value = []
        for x in df1[x_key].unique():
            y = df1[df[x_key] == x].min()[y_key]
            if y is np.nan:
                continue
            if y == 0:
                continue
            current_domain.append(x)
            current_value.append(y)
        # current_domain, current_value = zip(*sorted(zip(current_domain, current_value)))
        # lines.append({'label': "{}={}".format(tag_key, label), 'domain': current_domain, 'range': current_value})
        if label in tag_map:
            label = tag_map[label]
        lines.append({'label': label, 'domain': current_domain, 'range': current_value})

    print(lines)
    # plotexp.line_plot(title, x_key, y_key, lines, 'line1.png', False, is_show=False)
    plotexp.line_plot(title, "core number", "total time (s)", lines, 'draw/kcount_primitive.png', False, is_show=False)

if __name__ == "__main__":
    # draw_version()
    draw_primitive()
