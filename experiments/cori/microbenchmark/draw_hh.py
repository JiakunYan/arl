import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import sys
sys.path.append('../../include')
import matplotlib.pyplot as plt

pd.set_option('display.float_format',lambda x : '%.3f' % x)

data = None
# columns = ['core_num', 'handler_time', 'average_time']
title = "System overhead with different handlers"
x_key = "handler_time"
y_key = 'total_time'

def draw_task(task_name):
    df = pd.read_csv("data/heavy_handler.csv")
    # print(df)
    lines = []

    labels = []
    values1 = []
    values2 = []
    for x in df[x_key].unique():
        criterion = (df[x_key] == x) & (df["task"]==task_name) & (df["node_num"]==32)
        y = df[criterion].mean()[y_key]
        if y is np.nan:
            continue
        labels.append(str(x))
        values1.append(y-x)
        values2.append(x)
    labels, values1, values2 = zip(*sorted(zip(labels, values1, values2), key=lambda x: x[2]))

    fig, ax = plt.subplots()
    ax.set(title=title, xlabel="time inside handler (us)", ylabel="total time (us)")
    p1 = ax.bar(labels, values1)
    p2 = ax.bar(labels, values2, bottom=values1)
    for a, b1, b2 in zip(labels, values1, values2):
        ax.text(a, b1, '%d' % b1, ha='center', va='bottom', fontsize=11)
        ax.text(a, b1 + max(b2, 50), '%d' % (b1+b2), ha='center', va='bottom', fontsize=11)
    ax.legend((p1[0], p2[0]), ("system overhead (us)", "time inside handler (us)"))
    # plt.show()
    plt.savefig("draw/heavy_handler_{}.png".format(task_name))
    return labels, values1, values2

if __name__ == "__main__":
    total_labels = []
    total_value1 = []
    total_value2 = []
    labels, value1, value2 = draw_task("rpc_ffrd")
    labels = list(map(lambda x: "arl_{}".format(x), labels))
    total_labels.extend(labels)
    total_value1.extend(value1)
    total_value2.extend(value2)
    labels, value1, value2 = draw_task("aggr_store")
    labels = list(map(lambda x: "upcxx_{}".format(x), labels))
    total_labels.extend(labels)
    total_value1.extend(value1)
    total_value2.extend(value2)
    labels, value1, value2 = draw_task("GASNet")
    labels = list(map(lambda x: "gex_{}".format(x), labels))
    total_labels.extend(labels)
    total_value1.extend(value1)
    total_value2.extend(value2)

    fig, ax = plt.subplots()
    ax.set(title=title, xlabel="time inside handler (us)", ylabel="total time (us)")
    p1 = ax.bar(total_labels, total_value1)
    p2 = ax.bar(total_labels, total_value2, bottom=total_value1)
    ax.set(aspect=1.0 / ax.get_data_ratio() * 0.3)
    ax.set_xticks([])
    # plt.show()
    plt.savefig("draw/heavy_handler_all.png")