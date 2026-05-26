/*
 * BAM (binary angle measurement) represents angles as n-bits:
 *  - In BAM, each bit represents a fraction of 180 degrees, with the MSB representing 180 degrees
 *    and the LSB representing the smallest angle (180 degrees * 2^-(n-1))
 *  - There are 2^n units per circle
 *      - 2^(n-1) represents pi rad or 180 degrees
 *      - 2^(n-2) represents pi/2 rad or 90 degrees
 *  - You can add and subtract BAM angles without worrying about overflow
 *  - You can multiply BAM angles by constants
 *  - Adding 180 degrees is equivalent to flipping the MSB
 *  - The quadrant is given directly by the two MSBs
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "bam.h"

int main()
{
    FILE *fp = fopen("sin.bin", "wb");
    FILE *fp1 = fopen("real_sin.bin", "wb");

    int16_t *sample_buf = malloc(sizeof(int16_t) * 361);
    int16_t *real_sin_buf = malloc(sizeof(int16_t) * 361);

    for (int i = 0; i <= 360; i++)
    {
        bam_t angle = float_deg_to_bam(i);
        //printf("sin(%d BAM or %f degrees) = %f\n", angle, bam_to_float_deg(angle), ((float) bam_sin(angle)) * pow(2, -8));
        sample_buf[i] = bam_sin(angle);
        real_sin_buf[i] = sin(i * M_PI / 180) * pow(2, 8);

        printf("\ti = %d, rad = %f\n", i, i * M_PI / 180);
        printf("sample_buf[%d] = %d\n", i, sample_buf[i]);
        printf("real_sin_buf[%d] = %d\n", i, real_sin_buf[i]);
    }

    fwrite(sample_buf, sizeof(int16_t), 361, fp);

    fclose(fp);

    fwrite(real_sin_buf, sizeof(int16_t), 361, fp1);

    fclose(fp1);
    

    /*
    // Creating BAM angle from degrees
    bam_t bam_angle = float_deg_to_bam(359);

    printf("bam_angle = %d units or %f degrees\n", bam_angle, bam_to_float_deg(bam_angle));

    // Creating BAM angle from pi radians
    bam_angle = 3 * BAM_PI_RAD / 4;

    printf("bam_angle = %d units or %f degrees\n", bam_angle, bam_to_float_deg(bam_angle));

    // Adding two BAM angles, no need to worry about overflow
    bam_t tmp = bam_angle + BAM_180_DEG;

    printf("bam_angle + 180 degrees = %d units or %f degrees\n", tmp, bam_to_float_deg(tmp));
    */
}

float bam_to_float_deg(bam_t angle)
{   
    return (float) angle * 180 / BAM_180_DEG;
}

float ubam_to_float_deg(ubam_t angle)
{
    return (float) angle * 180 / UBAM_180_DEG;
}

bam_t float_deg_to_bam(float angle)
{
    return (ubam_t) (angle * BAM_180_DEG / 180);
}

ubam_t float_deg_to_ubam(float angle)
{
    return angle * UBAM_180_DEG / 180;
}
