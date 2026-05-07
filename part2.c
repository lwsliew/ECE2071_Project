#include <stdio.h>
#include <stdint.h>

typedef struct{
    char riff[4]; //"RIFF"
    uint32_t file_size;
    char wav[4]; //"WAVE"
    char fmt[4]; //"fmt"
    uint32_t fmtlen;
    uint16_t audioformat;
    uint16_t numchannel;
    uint32_t samplingrate;
    uint32_t bytesec;
    uint16_t bytesample;
    uint16_t bitssample;
    char header[4]; //"data"
    uint32_t totalsize;
} WAVheader;

int main() {
    FILE *inputFile = fopen("raw_ADC_values.data", "rb");
    FILE *outputFile = fopen("recorded_audio.wav", "wb");

    if (inputFile == NULL) {
        perror("Error opening input file");
        return 1;
    }

    //find length of file
    fseek(inputFile, 0, SEEK_END);
    int input_file_size = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET); // Reset pointer to the beginning

    uint32_t num_samples = input_file_size / sizeof(uint16_t);
    uint32_t data_payload_size = num_samples * sizeof(int16_t);

    if (!inputFile || !outputFile) {
        printf("Error: Could not open files.\n");
        return 1;
    }

    WAVheader header = {
        .riff = {'R', 'I', 'F', 'F'},
        .file_size = data_payload_size + 44 - 8, //minus riff and size
        .wav = {'W', 'A', 'V', 'E'},
        .fmt = {'f', 'm', 't', ' '},
        .fmtlen = 16,
        .audioformat = 1,
        .numchannel = 1,
        .samplingrate = 6400,
        .bytesec = 12800, //(6400*16*1)/8,
        .bytesample = 2, //(16*1)/8,
        .bitssample = 16,
        .header = {'d','a','t','a'}, //"data"
        .totalsize = data_payload_size
    };

    fwrite(&header, sizeof(WAVheader), 1, outputFile);

    uint16_t raw_sample;

    int count = 0;
    while (fread(&raw_sample, sizeof(uint16_t), 1, inputFile)) {
        count++;
        int16_t scaled_sample = (int16_t)((raw_sample - 2048) * 16);
        fwrite(&scaled_sample, sizeof(int16_t), 1, outputFile);
    }
    printf("Processed %d samples.\n", count);

    fclose(inputFile);
    fclose(outputFile);
    return 0;
}