import sqlglot

TEMPLATE = """
SELECT c.name, o.region, SUM(l.price * (1 - l.discount)) AS revenue, COUNT(DISTINCT o.id) AS orders_{i}
FROM customers AS c
JOIN orders AS o ON o.customer_id = c.id AND o.created >= DATE '2024-01-01'
JOIN lineitems AS l ON l.order_id = o.id
LEFT JOIN (SELECT order_id, MAX(shipped) AS shipped FROM shipments GROUP BY order_id) AS s ON s.order_id = o.id
WHERE c.segment IN ('a', 'b', 'c') AND l.quantity BETWEEN {i} AND {j}
  AND (s.shipped IS NULL OR s.shipped < o.created + INTERVAL '30' DAY)
GROUP BY c.name, o.region HAVING SUM(l.price) > {j} ORDER BY revenue DESC LIMIT 50
"""
trees = []
out = 0
for i in range(4000):
    tree = sqlglot.parse_one(TEMPLATE.format(i=i, j=i + 7))
    trees.append(tree)
    out += len(tree.sql(dialect='duckdb')) + len(tree.sql(dialect='bigquery'))
print(len(trees), out)
