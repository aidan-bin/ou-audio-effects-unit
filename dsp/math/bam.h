#ifndef BAM_H
#define BAM_H

#include <stdint.h>

// Signedness determines range of degrees (necessary for converting between degrees and BAM)
typedef int16_t bam_t;      // Signed BAM type (-180,180]
typedef uint16_t ubam_t;    // Unsigned BAM type [0,360)

#define BAM_N 16                                            // Number of bits used (note: must match bam_t)
#define BAM_180_DEG ((bam_t) ((bam_t) 1 << (BAM_N - 1)))    // Number of BAM units in 180 degrees
#define BAM_PI_RAD BAM_180_DEG                              // Number of BAM units in pi radians (= 180 degrees)
#define UBAM_180_DEG ((ubam_t) BAM_180_DEG)                 // Number of BAM units in 180 degrees
#define UBAM_PI_RAD UBAM_180_DEG                            // Number of BAM units in pi radians (= 180 degrees)
#define BAM_MAX_ANGLE ((bam_t) -1)                          // Max angle that can be represented in BAM before wrapping back to 0
#define UBAM_MAX_ANGLE ((ubam_t) -1)                        // Max angle that can be represented in BAM before wrapping back to 0

#define BAM_PLUS_180_DEG(angle) (angle ^ BAM_180_DEG)

#ifndef FIXED_POINT_Q
#define FIXED_POINT_Q 8   // N in QN for fixed-point numbers
#endif

float bam_to_float_deg(bam_t angle);    // BAM angle to float degrees
float ubam_to_float_deg(ubam_t angle);  // UBAM angle to float degrees
bam_t float_deg_to_bam(float angle);    // Float degrees to BAM angle
ubam_t float_deg_to_ubam(float angle);  // Float degrees to UBAM angle

#endif