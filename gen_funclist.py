# mfw try using the parsing library and it fails

import re
from collections import OrderedDict

with open("src/web.c", "r") as f:
    data = f.read()

pat = r"GAME_[a-z0-9_]+(?=\()"

matches = re.findall(pat, data)
matches = list(OrderedDict.fromkeys(matches))

with open("emcc_funclist.txt", "w") as f:
    f.write("-sEXPORTED_FUNCTIONS=_" + ",_".join(matches) + "\n")

with open("js_funclist.txt", "w") as f:
    f.write("const FUNCLIST = [\n")
    for m in matches:
        f.write(f'    ["{m}", "number", ["number"]],\n')
    f.write("];\n")
