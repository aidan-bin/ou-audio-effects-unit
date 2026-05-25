/*
 * Command format: generate_samples <out filename> <number of samples> <signal type> <parameters list>
 *
 * Signal type (parameters):
 *  - sin (freq in samples/cycle) (amplitude) (phase in samples) 
 *  - random (amplitude)
 *  - constant (value)
 *  - impulse (value) (delay)
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define X_AXIS (UINT16_MAX / 2)     // x-axis is the median representable signal

int main(int argc, char *argv[])
{
    // Validate arguments
    if (argc < 3)
    {
        fprintf(stderr, "Error: Invalid number of arguments");
        return 0;
    }

    const int num_samples = atoi(argv[2]);

    // Open output file
    FILE *fp;

    if (!(fp = fopen(argv[1], "wb")))
    {
        fprintf(stderr, "Error: Couldn't open file");
        return 0;
    }

    uint16_t *sample_buf = malloc(sizeof(uint16_t) * num_samples);

    // Generate samples
    if (strcmp(argv[3], "sin") == 0)
    {
        if (argc != 7)
        {
            fprintf(stderr, "Error: Invalid argument count");
            return 0;
        }

        double freq_samples_per_cycle;
        double freq_rad_per_sample;
        sscanf(argv[4], "%lf", &freq_samples_per_cycle);
        freq_rad_per_sample = 2 * M_PI / freq_samples_per_cycle;
        
        double amplitude;
        sscanf(argv[5], "%lf", &amplitude);

        double phase_rad;
        sscanf(argv[6], "%lf", &phase_rad);

        printf("Generating %d samples of sin wave with:\n\tFrequency (samples/cycle) = %lf\n\tAmplitude = %lf\n\tPhase (rad) = %lf",
            num_samples, freq_samples_per_cycle, amplitude, phase_rad);

        for (int sample_index = 0; sample_index < num_samples; sample_index++)
        {
            double sin_val = amplitude * sin(freq_rad_per_sample * sample_index + phase_rad);
            //printf("sin_val(sample = %d) = %lf\n", sample_index, sin_val);

            // Saturate sin_val (get it in range [-UINT64_MAX / 2, UINT64_MAX / 2])
            double max = (double) X_AXIS;

            if (sin_val < -max)
            {
                sin_val = -max;
            }
            else if (sin_val > max)
            {
                sin_val = max;
            }

            // Shift sin_val up so min is 0
            sample_buf[sample_index] = (uint16_t) (sin_val + max);
            //printf("sample_buf(sample = %d) = %u\n", sample_index, sample_buf[sample_index]);
        }
    }
    else if (strcmp(argv[3], "random") == 0)
    {
        if (argc != 5)
        {
            fprintf(stderr, "Error: Invalid argument count");
            return 0;
        }

        srand((unsigned int) time(NULL));

        double amplitude;
        sscanf(argv[4], "%lf", &amplitude);

        printf("Generating %d samples of random values with:\n\tAmplitude = %lf", num_samples, amplitude);

        for (int sample_index = 0; sample_index < num_samples; sample_index++)
        {
            double rand_num = (double) rand() / RAND_MAX;

            rand_num *= amplitude;

            double max = (double) X_AXIS;

            rand_num += max;

            // Saturate sum
            if (rand_num > (int) UINT16_MAX)
            {
                rand_num = UINT16_MAX;
            }

            sample_buf[sample_index] = (uint16_t) rand_num;
        }
    }
    else if (strcmp(argv[3], "constant") == 0)
    {
        if (argc != 5)
        {
            fprintf(stderr, "Error: Invalid argument count");
            return 0;
        }

        double constant;
        sscanf(argv[4], "%lf", &constant);

        printf("Generating %d samples of constant values with:\n\tConstant = %lf", num_samples, constant);

        double max = (double) X_AXIS;

        // Offset constant
        constant += max;

        //printf("constant (offset) = %lf\n", constant);

        for (int sample_index = 0; sample_index < num_samples; sample_index++)
        {
            // Saturate constant
            if (constant < 0)
            {
                constant = 0;
            }
            else if (constant > (int) UINT16_MAX)
            {
                constant = (int) UINT16_MAX;
            }

            sample_buf[sample_index] = (uint16_t) constant;
            //printf("sample_buf[sample_index] = %u\n", sample_buf[sample_index]);   
        }
    }
    else if (strcmp(argv[3], "impulse") == 0)
    {
        if (argc != 6)
        {
            fprintf(stderr, "Error: Invalid argument count");
            return 0;
        }

        double value;
        sscanf(argv[4], "%lf", &value);

        size_t delay;
        sscanf(argv[5], "%u", &delay);

        printf("Generating %d samples of impulse signal with:\n\tValue = %lf\n\tDelay = %u", num_samples, value, delay);

        sample_buf[delay] = (uint16_t) value + X_AXIS;

        for (size_t sample_index = 0; sample_index < num_samples; sample_index++)
        {
            if (sample_index != delay)
                sample_buf[sample_index] = 0 + X_AXIS;
        }
    }
    else
    {
        fprintf(stderr, "Error: Unrecognized signal type");
        return 0;
    }

    fwrite(sample_buf, sizeof(uint16_t), num_samples, fp);

    fclose(fp);
}
