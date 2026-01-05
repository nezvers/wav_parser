#include <stdio.h>
#include <math.h>

#define WAV_IMPLEMENTATION
#include "../wav.h"


void write_wav(WAVFile *wav_file, const char *filename){
    FILE* file_write = fopen(filename, "wb");
    WavWriteToFile(file_write, wav_file->header, wav_file->buffer.data, wav_file->sample_count);
    fclose(file_write);
}

void use_wav(WAVFile *wav_file){
    int channel_count = wav_file->header.channel_count;
    int sample_count = wav_file->sample_count / channel_count;
    float* samples = wav_file->buffer.data;

    for (int s = 0; s < sample_count; s++){
        if (channel_count == 1){
            float sample = samples[s * channel_count];
            printf("%s: %f\n", "Mono", sample);
            continue;
        }
        for (int c = 0; c < channel_count; c++){
            float sample = samples[s * channel_count + c];
            printf("%s: %f\n", c==0 ? "L" : "R", sample);
        }
    }
}

void read_wav(){
    // From file
    WAVFile wav_file;
    FILE* fp = fopen("resources/mono_16_pcm.wav", "rb");
    if (WavGetFileProperty(&wav_file, fp) != 0){
        fclose(fp);
        return;
    }

    if (WavLoadBuffer(&wav_file, fp) != 0){
        fclose(fp);
        return;
    }
    fclose(fp);

    // From memory
    const uint8_t *wav_memory = wav_file.buffer.start;
    WAVFile wav_buffer;
    if (WavGetBufferProperty(&wav_buffer, wav_memory) != 0){
        return;
    }

    write_wav(&wav_file, "test/test_write.wav");
    
    use_wav(&wav_file);

    WavCleanup(&wav_file);

    return;
}

double lerp(double a, double b, double t){
    return a + (b - a) * t;
}

void generate_oscilator(float* buffer, int size, int sample_rate, double freq){
    double t;
    double t_increment = 1.0 / sample_rate;
    const double PI = 3.14159265358979323846;
    const double TAU = PI * 2.0;

    for (int i = 0; i < size; i++){
        double freq_2 = lerp(freq, freq * 2.0, t);
        buffer[i] = sin(t * TAU * freq + sin(t * TAU * freq_2) * PI);
        t += t_increment;
    }
}

int main(){
    read_wav();

    float samples[44100];
    const int sample_rate = 44100;
    generate_oscilator(samples, 44100, sample_rate, 90.0);

    WAVHeader wav_header = WavCreateHeader(sizeof(samples), 1, sample_rate);
    FILE* fp = fopen("test/output_32.wav", "wb");
    WavWriteToFile(fp, wav_header, samples, 44100);
    fclose(fp);

    return 0;
}