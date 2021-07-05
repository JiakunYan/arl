#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter, FormatStrFormatter

def line_plot(title, xlabel, ylabel, data, fname='out.pdf', add_perfect=True, is_show=False):
    fig, ax = plt.subplots()

    domain = set()
    lrange = set()

    for datum in data:
        for x in datum['domain']:
            domain.add(x)
        for y in datum['range']:
            lrange.add(y)

    domain = sorted(domain)

    for datum in data:
        marker='.'
        linestyle=None
        color=None
        if 'marker' in datum:
            marker = datum['marker']
        if 'linestyle' in datum:
            # print('Setting linestyle for %s to %s' % (datum['label'], datum['linestyle']))
            linestyle = datum['linestyle']
        if 'color' in datum:
            color = datum['color']
        ax.loglog(datum['domain'], datum['range'], label=datum['label'], marker=marker, linestyle=linestyle, color=color, markerfacecolor='white')

    if add_perfect is True:
        perfect_range = datum['range'][0] / (np.array(domain) / domain[0])
        for y in perfect_range:
            lrange.add(y)
        ax.loglog(domain, perfect_range, label='perfect scaling', linestyle='--', color='grey')

    ymin = min(lrange)
    ymax = max(lrange)

    ytick_range = list([2**x for x in range(-5, 40)])
    yticks = []
    for tick in ytick_range:
        if tick >= ymin and tick <= ymax:
            yticks.append(tick)

    ax.minorticks_off()
    ax.set_xticks(domain)
    ax.set_xticklabels(list(map(lambda x: str(x), domain)))
    ax.set_yticks(yticks)
    ax.yaxis.set_major_formatter(FormatStrFormatter('%.1f'))

    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.legend(loc='best')
    plt.tight_layout()
    if is_show:
        plt.show()
    plt.savefig(fname)
