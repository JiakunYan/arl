import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import sys
sys.path.append('../../include')
import matplotlib.pyplot as plt

pd.set_option('display.float_format',lambda x : '%.3f' % x)

data = None
# columns = ['core_num', 'handler_time', 'average_time']
title = "Attentiveness Microbenchmark"
x_key = "compute_us"
y_key = 'latency'

def draw_task(task_name):
    df = pd.read_csv("data/attentiveness.csv")
    # print(df)
    lines = []

    labels = []
    values1 = []
    values2 = []
    for x in df[x_key].unique():
        criterion = (df[x_key] == x) & (df["task"]==task_name)
        y = df[criterion].mean()[y_key]
        if y is np.nan:
            continue
        labels.append(str(x))
        values1.append(y-x)
        values2.append(x)
    labels, values1, values2 = zip(*sorted(zip(labels, values1, values2), key=lambda x: x[2]))

    fig, ax = plt.subplots()
    ax.set(title=title, xlabel="computation time (us)", ylabel="total time (us)")
    p1 = ax.bar(labels, values1)
    p2 = ax.bar(labels, values2, bottom=values1)
    for a, b1, b2 in zip(labels, values1, values2):
        ax.text(a, b1, '%d' % b1, ha='center', va='bottom', fontsize=11)
        ax.text(a, b1 + max(b2, 50), '%d' % (b1+b2), ha='center', va='bottom', fontsize=11)
    ax.legend((p1[0], p2[0]), ("system overhead (us)", "computation time (us)"))
    # plt.show()
    plt.savefig("draw/attentiveness_{}.png".format(task_name))

if __name__ == "__main__":
    draw_task("upcxx")
    draw_task("arl")