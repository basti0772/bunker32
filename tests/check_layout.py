"""Static asset/map checks, complementary to the C++ device self-test."""
import re
from collections import deque
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / 'firmware/Bunker32/Game.cpp').read_text()
rows = re.findall(r'"([0-4]{16})"', source)
assert len(rows) == 16
queue = deque([(3, 2)])
seen = set(queue)
while queue:
    x, y = queue.popleft()
    for a, b in [(x+1,y), (x-1,y), (x,y+1), (x,y-1)]:
        if 0 <= a < 16 and 0 <= b < 16 and rows[b][a] == '0' and (a,b) not in seen:
            seen.add((a,b))
            queue.append((a,b))
assert len(seen) == sum(row.count('0') for row in rows)
sprites = re.findall(r'"([ HSUYXGB]{10,})"', (root / 'firmware/Bunker32/Renderer.cpp').read_text())
assert len(sprites) == 24
assert all(len(row) == 16 for row in sprites)
print(f'PASS: all {len(seen)} floor cells reachable; 24 sprite rows valid')
