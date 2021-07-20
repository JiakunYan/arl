import sys,os
sys.path.append("../../include")
from parser_graph import ParserNode, ParserGraph, file_to_lines

name = "bw_strong"
input_path = "run/slurm_output.bw_strong-o*"
output_path = "data/"
if __name__ == "__main__":
    node0 = ParserNode(
        "srun -N=(\d+) --ntasks-per-node=\d+ (.+)",
        ["node_num", "task"],
        name = "srun"
    )

    node1 = ParserNode(
        '''req payload size is (\d+) Byte
           .+ overhead is (.+) us \(total .+ s\)
           Total MB to send is (\d+) MB
           Total single-direction node bandwidth \(req/gross\): (.+) MB/s
           Total single-direction node bandwidth \(req/pure\): (.+) MB/s''',
        ["payload_size", "overhead", "total_data", "gross_bw", "pure_bw"],
        name="arl"
    )

    node2 = ParserNode(
        '''req payload size = (\d+) Byte
           aggr_store overhead is (.+) us \(total .+ s\)
           Total MB to send is (\d+) MB
           Total single-direction node bandwidth \(req/pure\): (.+) MB/s''',
        ["payload_size", "overhead", "total_data", "pure_bw"],
        name="upcxx"
    )

    node3 = ParserNode(
        '''Total MB to send is (\d+) MB
           Node single-direction bandwidth = (.+) MB/s''',
        ["total_data", "pure_bw"],
        name="gex"
    )

    node0.connect([node1, node2, node3])

    all_labels = ["node_num", "task", "payload_size", "overhead", "gross_bw", "pure_bw"]

    lines = file_to_lines(input_path)

    graph = ParserGraph(node0, all_labels)
    df = graph.parse(lines)

    df["node_num"] = df.apply(lambda x: x["node_num"] * 32, axis=1)
    df.rename(columns={"node_num": "core_num"}, inplace=True)

    df.sort_values(by=["core_num"], inplace=True)
    print(list(df.columns))
    print(df.shape)
    df.to_csv(os.path.join(output_path, "{}.csv".format(name)))
