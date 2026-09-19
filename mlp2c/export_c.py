#!/usr/bin/env python3
"""학습된 가중치를 MCU 용 C 헤더로 내보낸다.

입력  : npz  (W0,b0,W1,b1,... 순서, W 는 (out, in) 행 우선)
출력  : weights.h  — const float 배열과 mlp_t 접근자

float32 로 내보낸다. 정수 양자화는 이 저장소의 범위를 넘는다.
"""
import argparse
import numpy as np


def fmt(x):
    return f"{float(x):.8e}f"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("npz")
    ap.add_argument("-o", "--out", default="weights.h")
    ap.add_argument("--name", default="mlp_model")
    a = ap.parse_args()

    z = np.load(a.npz)
    k = 0
    Ws, bs = [], []
    while f"W{k}" in z:
        Ws.append(np.asarray(z[f"W{k}"], dtype=np.float32))
        bs.append(np.asarray(z[f"b{k}"], dtype=np.float32))
        k += 1
    if not Ws:
        raise SystemExit("W0 가 없다. 저장 형식을 확인할 것.")

    dims = [Ws[0].shape[1]] + [W.shape[0] for W in Ws]
    n_param = sum(int(W.size + b.size) for W, b in zip(Ws, bs))

    L = []
    L.append("/* export_c.py 가 생성한 파일. 직접 고치지 말 것. */")
    L.append(f"/* 구조 {'-'.join(map(str, dims))}  ·  파라미터 {n_param:,}  "
             f"·  Flash {n_param * 4:,} B */")
    L.append("#ifndef MLP_WEIGHTS_H")
    L.append("#define MLP_WEIGHTS_H")
    L.append('#include "mlp.h"')
    L.append("")
    L.append(f"#define MLP_N_LAYERS {len(Ws)}")
    L.append(f"#define MLP_N_IN     {dims[0]}")
    L.append(f"#define MLP_N_OUT    {dims[-1]}")
    L.append(f"#define MLP_N_PARAM  {n_param}")
    L.append("")
    L.append("static const int mlp_dims_[] = {" + ", ".join(map(str, dims)) + "};")
    L.append("")

    for i, (W, b) in enumerate(zip(Ws, bs)):
        L.append(f"/* layer {i} : {W.shape[1]} -> {W.shape[0]} */")
        L.append(f"static const float mlp_W{i}_[] = {{")
        flat = W.reshape(-1)
        for s in range(0, flat.size, 8):
            L.append("    " + ", ".join(fmt(v) for v in flat[s:s + 8]) + ",")
        L.append("};")
        L.append(f"static const float mlp_b{i}_[] = {{")
        for s in range(0, b.size, 8):
            L.append("    " + ", ".join(fmt(v) for v in b[s:s + 8]) + ",")
        L.append("};")
        L.append("")

    L.append("static const float *const mlp_Wp_[] = {"
             + ", ".join(f"mlp_W{i}_" for i in range(len(Ws))) + "};")
    L.append("static const float *const mlp_bp_[] = {"
             + ", ".join(f"mlp_b{i}_" for i in range(len(Ws))) + "};")
    L.append("")
    L.append(f"static inline mlp_t {a.name}(void)")
    L.append("{")
    L.append("    mlp_t m;")
    L.append(f"    m.n_layers = {len(Ws)};")
    L.append("    m.dims = mlp_dims_;")
    L.append("    m.W = mlp_Wp_;")
    L.append("    m.b = mlp_bp_;")
    L.append("    return m;")
    L.append("}")
    L.append("")
    L.append("#endif /* MLP_WEIGHTS_H */")

    with open(a.out, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")

    print(f"{a.out}  구조 {'-'.join(map(str, dims))}  "
          f"파라미터 {n_param:,}  Flash {n_param * 4:,} B")


if __name__ == "__main__":
    main()
