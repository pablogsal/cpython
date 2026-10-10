import networkx as nx

g = nx.barabasi_albert_graph(120_000, 3, seed=1)
parts = nx.number_connected_components(g)
tree = nx.minimum_spanning_tree(g)
dist = nx.single_source_shortest_path_length(g, 0)
core = nx.core_number(g)
print(g.number_of_edges(), parts, tree.number_of_edges(), max(dist.values()), max(core.values()))
