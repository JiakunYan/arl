import sys,os
sys.path.append("../../include")
from parser_graph import ParserNode, ParserGraph, file_to_lines

name = "heavy_handler"
input_path = "run/slurm_output.heavy_handler-o*"
output_path = "data/"
if __name__ == "__main__":
    node0 = ParserNode(
        "srun -N=(\d+) --ntasks-per-node=\d+ .+",
        ["node_num"],
        name = "srun"
    )

    node1 = ParserNode(
        '''sleep time is (\d+) us
        (.+) overhead is (.+) us \(total .+ s\)''',
        ["handler_time", "task", "total_time"],
    )

    node0.connect([node1])

    all_labels = ["node_num", "handler_time", "task", "total_time"]

    lines = file_to_lines(input_path)

    graph = ParserGraph(node0, all_labels)
    df = graph.parse(lines)

    df.sort_values(by=["handler_time"], inplace=True)
    print(list(df.columns))
    print(df.shape)
    df.to_csv(os.path.join(output_path, "{}.csv".format(name)))
