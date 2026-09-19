#include "mlp.h"

size_t mlp_scratch_floats(const mlp_t *m)
{
    int mx = 0;
    for (int i = 0; i <= m->n_layers; ++i) {
        if (m->dims[i] > mx) {
            mx = m->dims[i];
        }
    }
    return (size_t)mx * 2u;
}

void mlp_forward(const mlp_t *m, const float *in, float *out, float *scratch)
{
    int mx = 0;
    for (int i = 0; i <= m->n_layers; ++i) {
        if (m->dims[i] > mx) {
            mx = m->dims[i];
        }
    }

    float *cur = scratch;
    float *nxt = scratch + mx;

    for (int i = 0; i < m->dims[0]; ++i) {
        cur[i] = in[i];
    }

    for (int k = 0; k < m->n_layers; ++k) {
        const int n_in  = m->dims[k];
        const int n_out = m->dims[k + 1];
        const float *W  = m->W[k];
        const float *bb = m->b[k];
        const int last  = (k == m->n_layers - 1);

        float *dst = last ? out : nxt;

        for (int j = 0; j < n_out; ++j) {
            const float *row = &W[(size_t)j * (size_t)n_in];
            float acc = bb[j];
            for (int i = 0; i < n_in; ++i) {
                acc += row[i] * cur[i];
            }
            if (!last && acc < 0.0f) {
                acc = 0.0f;              /* ReLU */
            }
            dst[j] = acc;
        }

        if (!last) {
            float *t = cur; cur = nxt; nxt = t;
        }
    }
}
