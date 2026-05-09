#include <stdio.h>
#include <stdint.h>

typedef struct
{
    char riff[4];
    uint32_t file_size;
    char wav[4];
    char fmt[4];
    uint32_t fmtlen;
    uint16_t audioformat;
    uint16_t numchannel;
    uint32_t samplingrate;
    uint32_t bytesec;
    uint16_t bytesample;
    uint16_t bitssample;
    char header[4];
    uint32_t totalsize;
} WAVheader;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Not enough command line arguments\n");
        return 1;
    }

    FILE *inputFile = fopen(argv[1], "rb");
    FILE *outputFile = fopen(argv[2], "wb");
    if (!inputFile || !outputFile)
    {
        return 1;
    }

    fseek(inputFile, 0, SEEK_END);
    int input_file_size = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    // 2 bytes per sample now!
    uint32_t num_samples = (input_file_size / 3) * 2;
    uint32_t data_payload_size = num_samples * 2;

    WAVheader header = {
        .riff = {'R', 'I', 'F', 'F'},
        .file_size = data_payload_size + 44 - 8,
        .wav = {'W', 'A', 'V', 'E'},
        .fmt = {'f', 'm', 't', ' '},
        .fmtlen = 16,
        .audioformat = 1,
        .numchannel = 1,
        .samplingrate = 44012, // NEW: 44.1 ksps
        .bytesec = 88024,      // NEW: 44100 * 2 bytes
        .bytesample = 2,       // NEW: 2 bytes per sample
        .bitssample = 16,      // Standard container for 12-bit data
        .header = {'d', 'a', 't', 'a'},
        .totalsize = data_payload_size};

    fwrite(&header, sizeof(WAVheader), 1, outputFile);

    int count = 0;

    uint8_t buffer[3];
    // Read 16-bit chunks, apply math to convert 12-bit unsigned to 16-bit signed
    while (fread(buffer, 1, 3, inputFile) == 3)
    {
        uint16_t a = ((uint16_t)buffer[0] << 4) | (buffer[1] >> 4);
        uint16_t b = (((uint16_t)buffer[1] & 0x0F) << 8) | buffer[2];

        int16_t sa = (int16_t)((a - 2048) * 16);
        int16_t sb = (int16_t)((b - 2048) * 16);

        fwrite(&sa, sizeof(int16_t), 1, outputFile);
        fwrite(&sb, sizeof(int16_t), 1, outputFile);
        count += 2;
    }

    printf("Processed %d samples at 44.1kHz.\n", count);
    fclose(inputFile);
    fclose(outputFile);
    return 0;
}