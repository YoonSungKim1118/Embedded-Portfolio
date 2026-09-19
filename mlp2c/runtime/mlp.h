/* mlp.h — MCU 에서 도는 고정 구조 MLP 추론기
 *
 * - 동적 할당 없음. 호출자가 scratch 를 준다.
 * - 은닉층 ReLU, 출력층 항등(회귀). 분류가 필요하면 호출측에서 argmax 를 쓴다.
 * - 가중치는 export_c.py 가 만든 헤더에 const 로 들어가 Flash 에 놓인다.
 */
#ifndef MLP_H
#define MLP_H

#include <stddef.h>

typedef struct {
    int                  n_layers;  /* 가중치 행렬 개수 (= 층 수 - 1) */
    const int           *dims;      /* [in, h1, ..., out], 길이 n_layers+1 */
    const float *const  *W;         /* W[k] 는 (dims[k+1] x dims[k]) 행 우선 */
    const float *const  *b;         /* b[k] 는 dims[k+1] */
} mlp_t;

/* scratch 는 부동소수 2 * max(dims) 개 이상이어야 한다.
 * mlp_scratch_floats() 로 필요한 개수를 구할 수 있다. */
size_t mlp_scratch_floats(const mlp_t *m);

void mlp_forward(const mlp_t *m, const float *in, float *out, float *scratch);

#endif /* MLP_H */
