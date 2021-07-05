import sys,os
sys.path.append("../../include")
from parser_graph import ParserNode, ParserGraph, file_to_lines

name = "latency"
input_path = "run/slurm_output.latency-o*"
output_path = "data/"
if __name__ == "__main__":
    node0 = ParserNode(
        "srun -N=(\d+) --ntasks-per-node=\d+ (.+)",
        ["node_num", "task"],
        name = "srun"
    )

    node1 = ParserNode(
        '''rpc latency is (.+) us \(total .+ s\)''',
        ["latency"],
        name="arl_upcxx"
    )

    node2 = ParserNode(
        '''AM latency is (.+) us \(total .+ s\)''',
        ["latency"],
        name="gex"
    )

    node0.connect([node1, node2])

    all_labels = ["node_num", "task", "latency"]

    lines = file_to_lines(input_path)

    graph = ParserGraph(node0, all_labels)
    df = graph.parse(lines)

    df["node_num"] = df.apply(lambda x: x["node_num"] * 32, axis=1)
    df.rename(columns={"node_num": "core_num"}, inplace=True)

    df.sort_values(by=["core_num"], inplace=True)
    print(list(df.columns))
    print(df.shape)
    df.to_csv(os.path.join(output_path, "{}.csv".format(name)))
