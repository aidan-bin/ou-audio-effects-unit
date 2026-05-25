/*
 * Command format: add_signals <out filename> <number of samples> <filename 1> <filename 2>
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // Validate arguments
    if (argc != 5)
    {
        fprintf(stderr, "Error: Invalid number of arguments");
        return 0;
    }

    const int num_samples = atoi(argv[2]);

    uint16_t *signal_1 = malloc(num_samples * sizeof(uint16_t));
    uint16_t *signal_2 = malloc(num_samples * sizeof(uint16_t));

    // Read in signals
    FILE *fp;

    // Read in signal 1
    if (!(fp = fopen(argv[3], "rb")))
    {
        fprintf(stderr, "Error: Couldn't open file");
        return 0;
    }

    size_t len = fread(signal_1, sizeof(uint16_t), num_samples, fp);

    if (len != num_samples)
    {
        fprintf(stderr, "Error: Error reading samples file");
        return 0;
    }

    fclose(fp);

    // Read in signal 2
    if (!(fp = fopen(argv[4], "rb")))
    {
        fprintf(stderr, "Error: Couldn't open file");
        return 0;
    }

    len = fread(signal_2, sizeof(uint16_t), num_samples, fp);

    if (len != num_samples)
    {
        fprintf(stderr, "Error: Error reading samples file");
        return 0;
    }

    fclose(fp);

    // Open output file
    if (!(fp = fopen(argv[1], "wb")))
    {
        fprintf(stderr, "Error: Couldn't open file");
        return 0;
    }

    // Add signals
    uint16_t *out_buf = malloc(num_samples * sizeof(uint16_t));

    for (int sample_index = 0; sample_index < num_samples; sample_index++)
    {
        uint32_t sum = signal_1[sample_index] + signal_2[sample_index];
        //printf("%u + %u = %u\n", signal_1[sample_index], signal_2[sample_index], sum);

        // Remove redundant offset
        sum -= UINT16_MAX / 2;

        // Saturate sum
        if (sum > UINT16_MAX)
        {
            sum = UINT16_MAX;
        }

        out_buf[sample_index] = (uint16_t) sum;
        //printf("out_buf[%d] = %u\n", sample_index, out_buf[sample_index]);
    }

    fwrite(out_buf, sizeof(uint16_t), num_samples, fp);

    fclose(fp);
}
