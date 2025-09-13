import graphviz

# Create a new directed graph
dot = graphviz.Digraph(comment='LCM Concepts Flowchart', format='png') # You can change format to 'svg', 'pdf', etc.

# Define node styles (optional)
node_style = {'shape': 'box', 'style': 'rounded'}
start_end_style = {'shape': 'oval'}
decision_style = {'shape': 'diamond'}
highlight_style = {'shape': 'box', 'style': 'filled', 'fillcolor': '#f9d', 'rounded': 'true'} # For PRE/LCM target
lcm_strategy_style = {'shape': 'box', 'style': 'filled', 'fillcolor': '#ccf', 'rounded': 'true'} # For LCM Strategy
challenge_style = {'shape': 'box', 'style': 'filled,dashed', 'fillcolor': '#fcc', 'rounded': 'true'} # For Challenge

# Define nodes with labels and styles
dot.node('A', 'Start: Program with Potential Redundancies', **start_end_style)
dot.node('B', 'Identify Computational Redundancy', **decision_style) # Using diamond for conceptual split
dot.node('C', 'Total Redundancy \n(Computed on ALL paths)', **node_style)
dot.node('D', 'Partial Redundancy \n(Computed on SOME paths)', **node_style)
dot.node('E', 'Solved by: \nCommon Subexpression Elimination (CSE) / Basic PRE', **node_style)
dot.node('F', 'Target for Advanced PRE / LCM', **highlight_style)
dot.node('G', 'Goal: Eliminate Redundancy Safely & Optimally', **node_style)
dot.node('H', 'Input: Dataflow Analysis', **node_style)
dot.node('I', 'Available Expressions \n(What *is* computed?)', **node_style)
dot.node('J', 'Anticipated Expressions \n(What *will be* computed?)', **node_style)
dot.node('K', 'Determine Placement Regions', **node_style)
dot.node('L', 'LCM Strategy: Find Latest Possible Safe Point', **lcm_strategy_style)
dot.node('M', 'Place computation "Lazily"', **node_style)
dot.node('N', 'Challenge: Critical Edges might require special handling', **challenge_style)
dot.node('O', 'Output: Optimized Code \n(Redundancy removed, minimized register pressure)', **node_style)
dot.node('P', 'End', **start_end_style)

# Define edges (connections)
dot.edge('A', 'B')
dot.edge('B', 'C')
dot.edge('B', 'D')
dot.edge('C', 'E')
dot.edge('D', 'F')
dot.edge('F', 'G')
dot.edge('G', 'H')
dot.edge('H', 'I')
dot.edge('H', 'J')
dot.edge('I', 'K')
dot.edge('J', 'K')
dot.edge('K', 'L')
dot.edge('L', 'M')
dot.edge('L', 'N')
dot.edge('M', 'O')
dot.edge('N', 'O')
dot.edge('E', 'O')
dot.edge('O', 'P')

# Render the graph to a file (e.g., 'lcm_flowchart.png')
# The view=True argument will try to open the generated file automatically
try:
    # The directory '.' means save in the current working directory
    dot.render('lcm_flowchart', directory='.', view=False)
    print("Flowchart 'lcm_flowchart.png' generated successfully in the current directory.")
except graphviz.backend.ExecutableNotFound:
    print("Error: Graphviz executable not found.")
    print("Please ensure Graphviz is installed and its 'bin' directory is in your system's PATH.")
except Exception as e:
    print(f"An error occurred during rendering: {e}")