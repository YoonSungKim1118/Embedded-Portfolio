#!/usr/bin/env python3
"""합성 데이터로 작은 MLP 를 학습하고, 가중치와 검증용 입출력을 저장한다.

NumPy 만 쓴다. 이 저장소의 목적은 학습 프레임워크가 아니라
"학습된 모델을 MCU 에서 같은 값으로 돌리는 것"이기 때문이다.

실제 연구에서 쓴 데이터는 피험자 데이터라 공개하지 않는다.
여기서는 같은 성격의 문제(다채널 센서 -> 연속값 회귀)를 합성으로 만든다.
"""
import numpy as np

RNG = np.random.default_rng(20260919)

N_IN, H1, H2, N_OUT = 8, 16, 12, 2
N_TRAIN, N_TEST = 4000, 500
EPOCHS, LR, BATCH = 400, 0.02, 64


def make_data(n):
    """8채널 센서 -> 2개 각도. 비선형이고 채널 간 상관이 있다."""
    x = RNG.normal(0.0, 1.0, size=(n, N_IN)).astype(np.float32)
    a = np.tanh(x[:, 0] + 0.5 * x[:, 1] - 0.3 * x[:, 2] * x[:, 3])
    b = 0.6 * np.sin(x[:, 4]) + 0.4 * x[:, 5] - 0.2 * x[:, 6] ** 2 + 0.1 * x[:, 7]
    y = np.stack([a, b], axis=1).astype(np.float32)
    return x, y


def init(fan_in, fan_out):
    s = np.sqrt(2.0 / fan_in)
    return (RNG.normal(0.0, s, size=(fan_out, fan_in)).astype(np.float32),
            np.zeros(fan_out, dtype=np.float32))


def main():
    xtr, ytr = make_data(N_TRAIN)
    xte, yte = make_data(N_TEST)

    W0, b0 = init(N_IN, H1)
    W1, b1 = init(H1, H2)
    W2, b2 = init(H2, N_OUT)

    for ep in range(EPOCHS):
        idx = RNG.permutation(N_TRAIN)
        for s in range(0, N_TRAIN, BATCH):
            j = idx[s:s + BATCH]
            x, y = xtr[j], ytr[j]
            m = len(j)

            z0 = x @ W0.T + b0;  a0 = np.maximum(z0, 0.0)
            z1 = a0 @ W1.T + b1; a1 = np.maximum(z1, 0.0)
            out = a1 @ W2.T + b2

            d = (out - y) * (2.0 / m)
            gW2 = d.T @ a1;  gb2 = d.sum(0)
            d1 = (d @ W2) * (z1 > 0)
            gW1 = d1.T @ a0; gb1 = d1.sum(0)
            d0 = (d1 @ W1) * (z0 > 0)
            gW0 = d0.T @ x;  gb0 = d0.sum(0)

            W2 -= LR * gW2; b2 -= LR * gb2
            W1 -= LR * gW1; b1 -= LR * gb1
            W0 -= LR * gW0; b0 -= LR * gb0

        if (ep + 1) % 100 == 0:
            p = np.maximum(np.maximum(xte @ W0.T + b0, 0) @ W1.T + b1, 0) @ W2.T + b2
            mse = float(((p - yte) ** 2).mean())
            r2 = float(1.0 - ((p - yte) ** 2).sum() / ((yte - yte.mean(0)) ** 2).sum())
            print(f"  epoch {ep + 1:4d}   MSE {mse:.5f}   R2 {r2:.4f}")

    # 검증용 샘플 — C 추론 결과와 대조한다
    xs = xte[:16]
    ps = np.maximum(np.maximum(xs @ W0.T + b0, 0) @ W1.T + b1, 0) @ W2.T + b2

    np.savez("model.npz",
             W0=W0, b0=b0, W1=W1, b1=b1, W2=W2, b2=b2)
    np.savez("refcase.npz", x=xs.astype(np.float32), y=ps.astype(np.float32))

    n_param = W0.size + b0.size + W1.size + b1.size + W2.size + b2.size
    print(f"\n구조 {N_IN}-{H1}-{H2}-{N_OUT}   파라미터 {n_param:,}   "
          f"Flash {n_param * 4:,} B")
    print("model.npz, refcase.npz 저장")


if __name__ == "__main__":
    main()
