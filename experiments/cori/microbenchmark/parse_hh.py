import sys,os
sys.path.append("../../include")
from parser_graph import ParserNode, ParserGraph, file_to_lines

name = "heavy_handler"
input_path = "run/slurm_output.heavy_handler-o*"
output_path = "data/"
if __name__ == "__main__":

    node1 = ParserNode(
        '''sleep time is (\d+) us
        (.+) overhead is (.+) us \(total .+ s\)''',
        ["handler_time", "task", "total_time"],
    )

    all_labels = ["handler_time", "task", "total_time"]

    lines = file_to_lines(input_path)

    graph = ParserGraph(node1, all_labels)
    df = graph.parse(lines)

    df.sort_values(by=["handler_time"], inplace=True)
    print(list(df.columns))
    print(df.shape)
    df.to_csv(os.path.join(output_path, "{}.csv".format(name)))
