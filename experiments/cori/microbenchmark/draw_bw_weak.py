import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import sys
sys.path.append('../../include')
import plotexp

pd.set_option('display.float_format',lambda x : '%.3f' % x)

data = None
# columns = ['core_num','task','payload_size','overhead','gross_bw','pure_bw']
title = "Microbenchmark"
x_key = "payload_size"
tag_key = "task"
y1_key = 'pure_bw'
y2_key = 'gross_bw'
# tag_mask = None
# tag_mask = ["kcount-upcxx", "kcount_ffrd"]
tag_mask = ["arl_agg_bw_weak", "arl_aggrd_bw_weak", "arl_ff_bw_weak", "arl_ffrd_bw_weak"]
# tag_mask = ["kcount-upcxx", "kcount_ffrd", "kcount-v0.3.1", "kcount-v0.3.0", "kcount-v0.2.0", "kcount-v0.1.0"]

upcxx_bw = 1106.21
gex_bw = 3534.913
pre_label = ["UPC++ (agg)"]
pre_value = [upcxx_bw]
pro_label = ["GASNet"]
pro_value = [gex_bw]

if __name__ == "__main__":
    df = pd.read_csv("data/bw_weak.csv")#[[tag_key, x_key, y_key, "payload_size"]]

    domain = []
    pure_bw = []
    overhead_bw = []
    for tag in tag_mask:
        criterion = (df[tag_key] == tag) & (df["core_num"] == 1024)
        df1 = df[criterion]
        current_domain = []
        current_value1 = []
        current_value2 = []
        for x in df1[x_key].unique():
            y1 = df1[df[x_key] == x].median()[y1_key]
            y2 = df1[df[x_key] == x].median()[y2_key]
            current_domain.append(x)
            current_value1.append(y1)
            current_value2.append(y2-y1)
        # current_domain, current_value = zip(*sorted(zip(current_domain, current_value)))
        # lines.append({'label': "{}={}".format(tag_key, label), 'domain': current_domain, 'range': current_value})
        current_domain, current_value1, current_value2 = zip(*sorted(zip(current_domain, current_value1, current_value2), key=lambda x: x[0]))
        labels = list(map(lambda x: str(x), current_domain))

        fig, ax = plt.subplots()
        ax.set(title=title, xlabel="payload size (Byte)", ylabel="bandwidth (MB/s)")
        p1 = ax.bar(pre_label+labels+pro_label, pre_value+list(current_value1)+pro_value)
        for a, b in zip(pre_label+labels+pro_label, pre_value+list(current_value1)+pro_value):
            ax.text(a, b + 0.05, '%d' % b, ha='center', va='bottom', fontsize=11)
        p2 = ax.bar(labels, current_value2, bottom=current_value1)
        for a, b in zip(labels, np.array(current_value2) + np.array(current_value1)):
            ax.text(a, b + 0.05, '%d' % b, ha='center', va='bottom', fontsize=11)
        ax.legend((p1[0], p2[0]), ("pure bandwidth (MB/s)", "system overhead (MB/s)"))
        # plt.show()
        plt.savefig("draw/bw_weak_{}.png".format(tag))

        labels = list(map(lambda x: tag[4:-2]+str(x), labels))
        domain.extend(labels)
        pure_bw.extend(current_value1)
        overhead_bw.extend(current_value2)

    fig, ax = plt.subplots()
    ax.set(title=title, xlabel="payload size", ylabel="bandwidth (MB/s)")
    p1 = ax.bar(pre_label+pro_label+domain, pre_value+pro_value+pure_bw)
    p2 = ax.bar(domain, overhead_bw, bottom=pure_bw)
    ax.set(aspect=1.0 / ax.get_data_ratio() * 0.3)
    ax.set_xticks([])
    # plt.show()
    plt.savefig("draw/bw_weak.png")
