#include "fast_math.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define TANH_CLAMP_POINT ((int16_t) (2.9858 * pow(2, FIXED_POINT_Q)))   // Point where tanh gets clamped to 1.0 in QA (should be where the error is lower than approximating)

/*
int16_t bam_sin(bam_t x)
{
    // sin(x) ~= x * ( (3 << q_P) - (4 * x * x >> r) ) >> s
    // bam_n : BAM width (2^n units/circle)
    // q_A   : QA for output
    // q_P   : QP for inside of parenthesis
    // r and s maintain proper binary point throughout

    static const int bam_n = BAM_N, q_A = FIXED_POINT_Q, q_P = 10;
    static const int r = 2 * (bam_n - 1) - q_P - 2;
    static const int s = bam_n - 1 - q_A + q_P;

    // Check quadrant, mirror if quadrant is 1 or 2
    //  Note: In BAM, quadrant is given directly by two MSBs (i.e., 1 -> 0b01, 2 -> 0b10)
    if ((int16_t) (x ^ (x << 1)) < 0)
        x = (1 << (bam_n - 1)) - x;  // Reflect across x = pi/2 (i.e., x = pi - x)

    return (int32_t) -x * ( (3L << q_P) - ((int32_t) x * x >> r) ) >> s;
}
*/

int16_t q_tanh(int16_t x)
{
    // Note: x and return value are in QA
    // Uses finite Lamber's series to 4 divisions (before simplification)

    if (x < -TANH_CLAMP_POINT) 
        return -(1 << FIXED_POINT_Q);

    if (x > TANH_CLAMP_POINT)
        return (1 << FIXED_POINT_Q);

    int32_t x_squared = (x * x) >> FIXED_POINT_Q;

    int32_t a = ( (10 * x * x_squared) >> FIXED_POINT_Q ) + 105 * x;
    int32_t b = ( (x_squared * x_squared) >> FIXED_POINT_Q) + (45 * x_squared) + (105 << FIXED_POINT_Q);

    return (a << FIXED_POINT_Q) / b;
}
/*
int main()
{
    FILE *fp_tanh = fopen("tanh.bin", "wb");
    FILE *fp_q_tanh = fopen("q_tanh.bin", "wb");

    const int sample_count = 500;
    
    int16_t *tanh_buf = malloc(sizeof(int16_t) * sample_count);
    int16_t *q_tanh_buf = malloc(sizeof(int16_t) * sample_count);

    // i is Q4
    //for (int i = (-4 << 4); i < (4 << 4); i++)
    //for (int i = -47; i == -47; i++)

    const float bound = 6.0f;

    float input = -bound;

    for (int i = 0; i < sample_count; i++)
    {
        tanh_buf[i] = (int16_t) (tanh(input) * pow(2, FIXED_POINT_Q));
        q_tanh_buf[i] = q_tanh((int16_t) (input * pow(2, FIXED_POINT_Q)));

        printf("\ti = %d, input = %lf\n", i, input);
        printf("tanh_buf[i] = %lf\n", (float) tanh_buf[i] * pow(2, -FIXED_POINT_Q));
        printf("q_tanh_buf[i] = %lf\n", (float) q_tanh_buf[i] * pow(2, -FIXED_POINT_Q));

        
        printf("i = %d, %f\n", i, (float) i * pow(2, -4));
        printf("tanh = %lf\n", tanh((float) i * pow(2, -4)));
        printf("approx. tanh = %lf\n", (float) q_tanh(i << (FIXED_POINT_Q - 4)) * pow(2, -FIXED_POINT_Q));
        printf("error = %lf\n", fabs(tanh((float) i * pow(2, -4)) - ((float) q_tanh(i << (FIXED_POINT_Q - 4)) * pow(2, -FIXED_POINT_Q))));
        
       //printf("(6.0f / 2.0f) / (sample_count / 2.0f) = %lf\n", (6.0f / 2.0f) / (sample_count / 2.0f));
       input += bound / (sample_count / 2.0f);
    }

    fwrite(tanh_buf, sizeof(int16_t), sample_count, fp_tanh);
    fwrite(q_tanh_buf, sizeof(int16_t), sample_count, fp_q_tanh);

    fclose(fp_tanh);
    fclose(fp_q_tanh);
}
*/
