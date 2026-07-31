import sys

def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

colors = {
    'R': rgb565(255, 0, 0),
    'G': rgb565(0, 255, 0),
    'B': rgb565(0, 0, 255),
    'Y': rgb565(255, 255, 0),
    'W': rgb565(255, 255, 255),
    'K': rgb565(0, 0, 0),
    'C': rgb565(0, 255, 255),
    'O': rgb565(255, 128, 0),
    'P': rgb565(255, 128, 255),
    'D': rgb565(128, 128, 128),
    'L': rgb565(200, 200, 200),
    'b': rgb565(100, 50, 0),
    '.': 0x0000
}

emojis = {
    'heart': [
        ". R R . R R . .",
        "R R R R R R R .",
        "R R R R R R R .",
        "R R R R R R R .",
        ". R R R R R . .",
        ". . R R R . . .",
        ". . . R . . . .",
        ". . . . . . . ."
    ],
    'warning': [
        ". . . . Y . . .",
        ". . . Y Y Y . .",
        ". . Y Y K Y Y .",
        ". Y Y Y K Y Y Y",
        "Y Y Y Y K Y Y Y",
        "Y Y Y Y . Y Y Y",
        "Y Y Y Y K Y Y Y",
        ". . . . . . . ."
    ],
    'trash': [
        ". D D D D D D .",
        "L L L L L L L L",
        ". L . L L . L .",
        ". L . L L . L .",
        ". L . L L . L .",
        ". L . L L . L .",
        ". L L L L L L .",
        ". . . . . . . ."
    ],
    'tooth': [
        ". W W . . W W .",
        "W W W W W W W W",
        "W W W W W W W W",
        "W W W W W W W W",
        ". W W . . W W .",
        ". W W . . W W .",
        ". W . . . . W .",
        ". . . . . . . ."
    ],
    'cloud': [
        ". . . . . . . .",
        ". . . . L L . .",
        ". . L L W W L .",
        ". L W W W W W L",
        "L W W W W W W W",
        "L L W W W W W L",
        ". L L L L L L .",
        ". . . . . . . ."
    ],
    'sun': [
        ". Y . . Y . . Y",
        ". . Y Y Y Y . .",
        "Y Y Y O O Y Y Y",
        ". Y O O O O Y .",
        ". Y O O O O Y .",
        "Y Y Y O O Y Y Y",
        ". . Y Y Y Y . .",
        ". Y . . Y . . Y"
    ],
    'party': [
        ". C C O O Y Y .",
        "C C W W W W Y Y",
        "C K W K W K W Y",
        ". W W W W W W .",
        ". . W K K W . .",
        ". . . R R . . .",
        ". . R R R R . .",
        ". . . . . . . ."
    ]
}

out = ""
for name, lines in emojis.items():
    out += f"const uint16_t EMOJI_{name.upper()}[64] PROGMEM = {{\n  "
    vals = []
    for line in lines:
        for char in line.split():
            vals.append(f"0x{colors[char]:04X}")
    out += ", ".join(vals)
    out += "\n};\n\n"

with open('emojis_data.txt', 'w') as f:
    f.write(out)

print("done")
