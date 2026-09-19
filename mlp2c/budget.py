#!/usr/bin/env python3
"""연산 예산에서 신경망 구조를 역산한다.

모델을 먼저 만들고 나중에 줄이는 대신, 타깃이 쓸 수 있는 클럭에서
파라미터 상한을 먼저 구하고 그 안에서 구조를 고른다.

    가용 클럭 = 코어 주파수 x 마감 시간
    파라미터 상한 = 가용 클럭 / MAC 당 사이클

부동소수 유닛이 없는 MCU 에서 MAC 하나가 몇 사이클인지는
컴파일러와 코어에 따라 달라지므로 --cycles-per-mac 으로 넘긴다.
보수적으로 잡는 편이 낫다. 여유가 남으면 나중에 늘리면 되지만,
모자라면 설계를 처음부터 다시 해야 한다.

사용 예
    python3 budget.py --mhz 120 --deadline-us 1000 --in 8 --out 2
"""
import argparse


def param_count(dims):
    """dims = [in, h1, ..., out] 의 가중치 + 편향 개수."""
    n = 0
    for a, b in zip(dims, dims[1:]):
        n += a * b + b
    return n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mhz", type=float, required=True, help="코어 주파수 (MHz)")
    ap.add_argument("--deadline-us", type=float, required=True,
                    help="추론 한 번의 마감 시간 (us)")
    ap.add_argument("--cycles-per-mac", type=float, default=1.0,
                    help="MAC 하나당 사이클 (기본 1.0, 보수적으로 올려 잡을 것)")
    ap.add_argument("--in", dest="n_in", type=int, required=True)
    ap.add_argument("--out", dest="n_out", type=int, required=True)
    ap.add_argument("--layers", type=int, default=2, help="은닉층 수 (1 또는 2)")
    ap.add_argument("--max-nodes", type=int, default=128)
    ap.add_argument("--top", type=int, default=10)
    a = ap.parse_args()

    cycles = a.mhz * 1e6 * (a.deadline_us * 1e-6)
    limit = cycles / a.cycles_per_mac

    print(f"코어            {a.mhz:g} MHz")
    print(f"마감            {a.deadline_us:g} us")
    print(f"가용 클럭       {cycles:,.0f}")
    print(f"MAC 당 사이클   {a.cycles_per_mac:g}")
    print(f"파라미터 상한   {limit:,.0f}")
    print()

    cands = []
    if a.layers == 1:
        for h in range(1, a.max_nodes + 1):
            dims = [a.n_in, h, a.n_out]
            p = param_count(dims)
            if p <= limit:
                cands.append((p, dims))
    else:
        for h1 in range(1, a.max_nodes + 1):
            for h2 in range(1, a.max_nodes + 1):
                dims = [a.n_in, h1, h2, a.n_out]
                p = param_count(dims)
                if p <= limit:
                    cands.append((p, dims))

    if not cands:
        print("상한 안에 들어가는 구조가 없다. 마감을 늘리거나 입력을 줄여야 한다.")
        return

    cands.sort(key=lambda t: -t[0])
    print(f"상한 안에서 가장 큰 구조 {min(a.top, len(cands))}개")
    print(f"{'구조':<28}{'파라미터':>10}{'여유':>10}")
    for p, dims in cands[:a.top]:
        shape = "-".join(str(d) for d in dims)
        print(f"{shape:<28}{p:>10,}{limit - p:>10,.0f}")

    print()
    p, dims = cands[0]
    print(f"권장  {'-'.join(str(d) for d in dims)}  (파라미터 {p:,})")
    print("실측으로 확인할 것. 위 계산은 MAC 만 세고 활성화·메모리 접근은 빼고 있다.")


if __name__ == "__main__":
    main()
