import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import sys,os
sys.path.append('../../include')
import plotexp

pd.set_option('display.float_format',lambda x : '%.3f' % x)

name = "latency"
input_path = "data/latency.csv"
output_path = "draw/"
# columns = ['core_num','task','payload_size','overhead','gross_bw','pure_bw']
x_key = "core_num"
tag_key = "task"
y_key = 'latency'
tag_mask = None
# tag_mask = ["kcount-upcxx", "kcount_ffrd"]
# tag_mask = ["kcount-upcxx", "kcount_ffrd", "kcount_ff", "kcount_aggrd", "kcount_agg"]
# tag_mask = ["kcount-upcxx", "kcount_ffrd", "kcount-v0.3.1", "kcount-v0.3.0", "kcount-v0.2.0", "kcount-v0.1.0"]

tag_map = {
    "arl_lt": "ARL",
    "upcxx_lt": "UPC++",
    "gex_lt": "GASNet"
}
if __name__ == "__main__":
    df = pd.read_csv(input_path)#[[tag_key, x_key, y_key, "payload_size"]]
    lines = []

    for tag in df[tag_key].unique():
        if tag_mask and tag not in tag_mask:
            continue
        if tag == "gex_bw":
            criterion = df[tag_key] == tag
        else:
            criterion = (df[tag_key] == tag)
        df1 = df[criterion]
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
        if tag in tag_map:
            tag = tag_map[tag]
        lines.append({'label': tag, 'domain': current_domain, 'range': current_value})

    print(lines)
    # plotexp.line_plot(title, x_key, y_key, lines, 'line1.png', False, is_show=False)
    title = "Microbenchmark for latency"
    output_file = os.path.join(output_path, 'microbenchmark_lt_core.png')
    plotexp.line_plot(title, "core number", "latency (us)", lines, output_file, False, is_show=False)
