/* Python 학습 결과와 C 추론 결과가 같은지 대조한다.
 * 임베디드 이식에서 가장 먼저 깨지는 곳이 이 지점이다 —
 * 가중치 순서, 행/열 방향, 활성화 적용 위치.
 */
#include "mlp.h"
#include "weights.h"
#include "refcase.h"

#include <math.h>
#include <stdio.h>

#define TOL 1e-4f

int main(void)
{
    mlp_t m = mlp_model();

    if (MLP_N_IN != REF_N_IN || MLP_N_OUT != REF_N_OUT) {
        printf("FAIL  모델과 검증 데이터의 차원이 다르다\n");
        return 1;
    }

    float scratch[256];
    if (mlp_scratch_floats(&m) > (sizeof(scratch) / sizeof(scratch[0]))) {
        printf("FAIL  scratch 가 모자란다\n");
        return 1;
    }

    float worst = 0.0f;
    int   fail  = 0;

    for (int n = 0; n < REF_N; ++n) {
        float out[MLP_N_OUT];
        mlp_forward(&m, &ref_x[(size_t)n * MLP_N_IN], out, scratch);

        for (int j = 0; j < MLP_N_OUT; ++j) {
            float want = ref_y[(size_t)n * MLP_N_OUT + j];
            float diff = fabsf(out[j] - want);
            if (diff > worst) {
                worst = diff;
            }
            if (diff > TOL) {
                printf("  FAIL  sample %d out %d : C %.6f  py %.6f  diff %.3e\n",
                       n, j, (double)out[j], (double)want, (double)diff);
                fail++;
            }
        }
    }

    printf("구조     ");
    for (int i = 0; i <= m.n_layers; ++i) {
        printf("%d%s", m.dims[i], (i == m.n_layers) ? "\n" : "-");
    }
    printf("파라미터 %d  (Flash %d B)\n", MLP_N_PARAM, MLP_N_PARAM * 4);
    printf("scratch  %zu floats\n", mlp_scratch_floats(&m));
    printf("샘플     %d개  최대 오차 %.3e  (허용 %.0e)\n",
           REF_N, (double)worst, (double)TOL);
    printf("%s\n", fail ? "FAILED" : "OK — Python 과 C 의 출력이 일치한다");
    return fail ? 1 : 0;
}
