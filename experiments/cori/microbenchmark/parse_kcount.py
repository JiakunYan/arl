import pandas as pd
import re
import glob
import ast
import numpy as np
import os

name = "kcount"
input_path = "run/slurm_output.kcount-o*"
output_path = "data/"
dir_name = "."
strings = [
    'srun -N=(\d+) --ntasks-per-node=\d+ (.+)',
    "Finished in (.+) s at .*",
]

column_names = [
    "node_num",
    "task",
    "total_time",
]

df = pd.DataFrame(columns=column_names)
fault_num = 0

def get_typed_value_(value):
    if value == '-nan':
        return np.nan
    try:
        typed_value = ast.literal_eval(value)
    except:
        typed_value = value
    return typed_value

def add_input(fname):
    global fault_num
    current_datum = []
    index = 0
    for line in open(fname, 'r'):
        string = strings[index]
        line = line.rstrip()
        m = re.match(string, line)
        if m:
            current_datum += [get_typed_value_(x) for x in m.groups()]
            index += 1
            # print('Setting current tuple to %s based on "%s"' % (current_datum,line))
            if index == len(strings): # reach end
                index = 0
                df.loc[len(df)] = current_datum
                current_datum = []
        else:
            m = re.match(strings[0], line)
            if m:
                index = 1
                fault_num += 1
                print("Parse {} error! {}".format(fname, current_datum))
                current_datum = [get_typed_value_(x) for x in m.groups()]
                # print('Setting current tuple to %s based on "%s"' % (current_datum,line))
                if index == len(strings): # reach end
                    index = 0
                    df.loc[len(df)] = current_datum
                    current_datum = []

def get_pure_overhead(x):
    pure_overhead_list = [
        'rand_us',
        'rpc_init_us',
        'agg_buf_ave_us',
        'gex_req_ave_us',
        'push_back_us',
        'barrier_ave_us',
        'future_get_us',
    ]
    try:
        return x[pure_overhead_list].sum()
    except:
        return np.nan

def rename_agg(x):
    if x["task"] == "agg":
        return "arl_agg"
    else:
        return x["task"]

if __name__ == "__main__":
    files = glob.glob(input_path)
    for file in files:
        add_input(file)
    print("fault number: {}".format(fault_num))

    df["node_num"] = df.apply(lambda x: x["node_num"] * 32, axis=1)
    df.rename(columns={"node_num": "core_num"}, inplace=True)
    df["task"] = df.apply(rename_agg, axis=1)

    df.sort_values(by=["core_num"], inplace=True)
    print(list(df.columns))
    print(df.shape)
    df.to_csv(os.path.join(output_path, "{}.csv".format(name)))
