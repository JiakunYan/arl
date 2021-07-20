import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import sys
sys.path.append('../../include')
import plotexp

pd.set_option('display.float_format',lambda x : '%.3f' % x)

data = None
# columns = ['core_num','task','payload_size','overhead','gross_bw','pure_bw']
x_key = "core_num"
tag_key = "task"
y_key = 'pure_bw'
total_data = 10000
tag_mask = None
# tag_mask = ["kcount-upcxx", "kcount_ffrd"]
# tag_mask = ["kcount-upcxx", "kcount_ffrd", "kcount_ff", "kcount_aggrd", "kcount_agg"]
# tag_mask = ["kcount-upcxx", "kcount_ffrd", "kcount-v0.3.1", "kcount-v0.3.0", "kcount-v0.2.0", "kcount-v0.1.0"]
tag_map = {
    "arl_ffrd_bw": "rpc_ffrd",
    "arl_ff_bw": "rpc_ff",
    "arl_aggrd_bw": "rpc_aggrd",
    "arl_agg_bw": "rpc_agg",
    "upcxx_agg_bw": "UPC++ (agg)",
    "gex_bw": "GASNet"
}

def main(total_data):
    df = pd.read_csv("data/bw_strong.csv")#[[tag_key, x_key, y_key, "payload_size"]]
    lines = []

    for tag in df[tag_key].unique():
        if tag_mask and tag not in tag_mask:
            continue
        if tag == "gex_bw":
            criterion = df[tag_key] == tag
        else:
            criterion = (df[tag_key] == tag) & (df["total_data"] == total_data)
        df1 = df[criterion]
        current_domain = []
        current_value = []
        for x in df1[x_key].unique():
            y = df1[df[x_key] == x].max()[y_key]
            if y is np.nan:
                continue
            if y == 0:
                continue
            current_domain.append(x)
            current_value.append(y)
        # current_domain, current_value = zip(*sorted(zip(current_domain, current_value)))
        # lines.append({'label': "{}={}".format(tag_key, label), 'domain': current_domain, 'range': current_value})
        if tag in tag_map:
            tag = tag_map[tag]
        lines.append({'label': tag, 'domain': current_domain, 'range': current_value})

    print(lines)
    # plotexp.line_plot(title, x_key, y_key, lines, 'line1.png', False, is_show=False)
    title = "Microbenchmark for bandwidth (strong scaling, {} MB in total)".format(total_data)
    plotexp.line_plot(title, "core number", "bandwidth (MB/s)", lines, 'draw/microbenchmark_bw_core_strong_{}mb.png'.format(total_data), False, is_show=False)

if __name__ == "__main__":
    main(10)
    main(100)
    main(1000)
    main(10000)
    main(100000)
