from PIL import Image
im = Image.open('emoticons.png')
print('size:', im.size)
# Save 16 individual cells for visual inspection
w, h = im.size
cell_w, cell_h = w // 4, h // 4
names = [
    'oop', 'exclamation', 'hearts', 'drop',
    'dotdot', 'music', 'sorry', 'ghost',
    'sushi', 'splattee', 'deviltee', 'zomg',
    'zzz', 'wtf', 'eyes', 'question',
]
for i, name in enumerate(names):
    col = i % 4
    row = i // 4
    cell = im.crop((col * cell_w, row * cell_h, (col + 1) * cell_w, (row + 1) * cell_h))
    cell.save(f'cell_{i:02d}_{name}.png')
print('saved 16 cells')
