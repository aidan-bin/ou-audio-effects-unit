/*
 * BAM (binary angle measurement) represents angles as n-bit values:
 * - In BAM, each bit represents a fraction of 180 degrees, with the MSB representing 180 degrees
 *   and the LSB representing the smallest angle (180 degrees * 2^-(n-1)).
 * - There are 2^n units per circle.
 *   - 2^(n-1) represents pi rad or 180 degrees.
 *   - 2^(n-2) represents pi/2 rad or 90 degrees.
 * - You can add and subtract BAM angles without worrying about overflow.
 * - You can multiply BAM angles by constants.
 * - Adding 180 degrees is equivalent to flipping the MSB.
 * - The quadrant is given directly by the two MSBs.
 */

#include "bam.h"

#define DEGREES_PER_HALF_TURN 180

float bam_to_float_deg(bam_t angle)
{
    return (float)angle * (float)DEGREES_PER_HALF_TURN / (float)(1U << (BAM_N - 1));
}

float ubam_to_float_deg(ubam_t angle)
{
    return (float)angle * (float)DEGREES_PER_HALF_TURN / UBAM_180_DEG;
}

bam_t float_deg_to_bam(float angle)
{
    return (bam_t)(angle * (float)(1U << (BAM_N - 1)) / (float)DEGREES_PER_HALF_TURN);
}

ubam_t float_deg_to_ubam(float angle)
{
    return (ubam_t)(angle * (float)UBAM_180_DEG / (float)DEGREES_PER_HALF_TURN);
}
