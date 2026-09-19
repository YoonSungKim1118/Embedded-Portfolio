#!/usr/bin/env python3
"""refcase.npz 를 C 배열 헤더로 바꾼다."""
import numpy as np

z = np.load("refcase.npz")
x, y = z["x"], z["y"]
L = ["/* 생성 파일. 직접 고치지 말 것. */",
     "#ifndef REFCASE_H", "#define REFCASE_H", "",
     f"#define REF_N      {x.shape[0]}",
     f"#define REF_N_IN   {x.shape[1]}",
     f"#define REF_N_OUT  {y.shape[1]}", ""]
for name, arr in (("ref_x", x), ("ref_y", y)):
    L.append(f"static const float {name}[] = {{")
    flat = arr.reshape(-1)
    for s in range(0, flat.size, 8):
        L.append("    " + ", ".join(f"{float(v):.8e}f" for v in flat[s:s + 8]) + ",")
    L.append("};")
L += ["", "#endif /* REFCASE_H */"]
open("refcase.h", "w", encoding="utf-8").write("\n".join(L) + "\n")
print("refcase.h 생성")
