#ifndef WAV_H
#define WAV_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

// WAV file header structure
typedef struct WAVHeader
{
    // Should contain the letters "RIFF"
    char file_id[4];

    // The size of entire file in bytes, minus 8 bytes for chunk_id and
    // chunk_size.
    uint32_t file_size;

    // Should contain the letters "WAVE"
    char format[4];

    // Should contain the letters "fmt "
    char subchunk_id[4];

    // 16 for PCM. This is the size of the rest of the sunchunk which follows this
    // number.
    uint32_t subchunk_size;

    // PCM = 1, values other than 1 indicate some form of compression
    // IEEE = 3, 32 bit
    uint16_t audio_format;

    // mono = 1, stereo = 2, etc.
    uint16_t channel_count;

    // self-explanatory
    uint32_t sample_rate;

    // sample_rate * number of channels * bits per sample / 8
    uint32_t byte_rate;

    // number of channels * bits per sample / 8. Number of bytes for one sample
    // including all channels.
    uint16_t block_align;

    // BITS, not BYTES.
    uint16_t bits_per_sample;

    // Should contain the letters "data"
    char data_id[4];

    // number of samples * number of channels * bits per sample / 8
    // Actual number of bytes in the sound data.
    uint32_t data_size;

} WAVHeader;

// Whole Wav file is allocated
typedef struct WAVBuffer {
    uint8_t* start;
    // Points at the the sample data offset
    float* data;
    // Size of sample data
    uint32_t data_size;
}WAVBuffer;

enum SampleFormat {
    WAV_U8_PCM, WAV_S16_PCM, WAV_S24_PCM, WAV_S32_PCM, WAV_32_FLOAT
};

// WAV file with header and pointer to data
typedef struct WAVFile
{
    // WAV header (copy re-constructed from the file with proper byte aligment)
    WAVHeader header;

    // Data offset in wav file
    uint32_t data_offset;

    // Extra precalculated values
    uint8_t byte_per_sample;

    // Array length
    uint32_t sample_count;

    // Sample format
    int sample_format;

    // Holds reference to wav file loaded in memory, should be freed if so
    WAVBuffer buffer;

} WAVFile;

#endif

#ifdef WAV_IMPLEMENTATION
#undef WAV_IMPLEMENTATION

/*
data_size: sample buffer size
channel_count: mono=1 / stereo=2 / ...
sample_rate: 44100, 48000, 96000
*/
WAVHeader WavCreateHeader(uint32_t data_size, uint16_t channel_count, uint32_t sample_rate) {
    WAVHeader header = (WAVHeader){0};
    memcpy((void*)header.file_id, (void*)"RIFF", 4);
    header.file_size = sizeof(header) - 8 + data_size;
    memcpy((void*)header.format, (void*)"WAVE", 4);
    memcpy((void*)header.subchunk_id, (void*)"fmt ", 4);
    header.subchunk_size = 16; // TODO: check if actually need to be calculated
    header.audio_format = 3; // TODO: Currently support only for 32-bit floats
    header.channel_count = channel_count;
    header.sample_rate = sample_rate;
    header.bits_per_sample = 32;
    header.byte_rate = sample_rate * channel_count * header.bits_per_sample / 8; // sample_rate * number of channels * bits per sample / 8
    header.block_align = (uint16_t)(channel_count * header.bits_per_sample / 8);
    memcpy((void*)header.data_id, (void*)"data", 4);
    header.data_size = data_size;

    return header;
}

int WavGetSampleFormat(WAVHeader header) {
    if (header.bits_per_sample == 32) {
        if (header.audio_format == 3) {
            return WAV_32_FLOAT;
        }
        else if (header.audio_format == 1) {
            return WAV_S32_PCM;
        }
    }
    if (header.bits_per_sample == 24) {
        if (header.audio_format == 1) {
            return WAV_S24_PCM;
        }
        else {
            // Other format?
        }
    }
    if (header.bits_per_sample == 16) {
        if (header.audio_format == 1) {
            return WAV_S16_PCM;
        }
        else {
            // Other format?
        }
    }

    return -1; // TODO: format not found
}

/** @returns true if the chunk of 4 characters matches the supplied string */
static bool ChunkIDMatches(char chunk[4], const char* chunkName)
{ // https://github.com/mhroth/tinywav/blob/4b0283d6b2a72afc5fec94b58b1536d6dc7c4691/tinywav.c#L56
    for (int i = 0; i < 4; ++i) {
        if (chunk[i] != chunkName[i]) {
            return false;
        }
    }
    return true;
}

int WavGetFileProperty(WAVFile* output, FILE* fp) {
    // https://github.com/mhroth/tinywav/blob/master/tinywav.c
    WAVHeader header;
    output->data_offset = 0;


    size_t cursor = fread(header.file_id, sizeof(char), 4, fp);// RIFF
    cursor += fread(&header.file_size, sizeof(uint32_t), 1, fp);
    cursor += fread(header.format, sizeof(char), 4, fp);// "WAVE"

    if (cursor < 9 || !ChunkIDMatches(header.file_id, "RIFF") || !ChunkIDMatches(header.format, "WAVE")) {
        // TODO: Use error code
        return -1;
    }


    while (fread(header.subchunk_id, sizeof(char), 4, fp) == 4) { // "fmt "
        fread(&header.subchunk_size, sizeof(uint32_t), 1, fp);
        if (ChunkIDMatches(header.subchunk_id, "fmt ")) {
            break;
        }
        else {
            fseek(fp, header.subchunk_size, SEEK_CUR); // skip this subchunk by moving file reading cursor
        }
    }

    // fmt Subchunk
    cursor = fread(&header.audio_format, sizeof(uint16_t), 1, fp);
    cursor += fread(&header.channel_count, sizeof(uint16_t), 1, fp);
    cursor += fread(&header.sample_rate, sizeof(uint32_t), 1, fp);
    cursor += fread(&header.byte_rate, sizeof(uint32_t), 1, fp);
    cursor += fread(&header.block_align, sizeof(uint16_t), 1, fp);
    cursor += fread(&header.bits_per_sample, sizeof(uint16_t), 1, fp);

    if (cursor != 6) {
        // TODO: Use error code
        return -1;
    }

    while (fread(header.data_id, sizeof(char), 4, fp) == 4) { // "data"
        fread(&header.data_size, sizeof(uint32_t), 1, fp);
        if (ChunkIDMatches(header.data_id, "data")) {
            break;
        }
        else {
            fseek(fp, header.data_size, SEEK_CUR); // skip this subchunk
        }
    }

    output->header = header;
    output->data_offset = ftell(fp);
    output->byte_per_sample = header.bits_per_sample / 8;
    output->sample_format = WavGetSampleFormat(header);

    output->sample_count = output->header.data_size / output->byte_per_sample;
    output->buffer = (WAVBuffer){0};

    return 0;
}

static inline long convert24to32(const uint8_t* byte) {
    long value = (byte[2] << 16) | (byte[1] << 8) | byte[0];
    // Sign-extend if the 24-bit number is negative (check if bit 23 is set)
    if (byte[2] & 0x80) {
        value |= 0xFF000000; // Set upper 8 bits to 1 for negative numbers
    }
    return value;
}

size_t buffer_read(uint8_t* target, size_t size, size_t count, uint8_t* source) {
    for (int i = 0; i < count; i++) {
        memcpy((void*)target, (void*)source, size);
        target += size;
        source += size;
    }
    return count;
}

// Create a context from wav file in memory.
int WavGetBufferProperty(WAVFile* output, const uint8_t* data) {
    uint8_t* data_ptr = (uint8_t*)data;
    WAVHeader header;
    output->data_offset = 0;


    size_t cursor = buffer_read((uint8_t*)header.file_id, sizeof(char), 4, data_ptr);// RIFF
    data_ptr += 4;
    cursor += buffer_read((uint8_t*)&header.file_size, sizeof(uint32_t), 1, data_ptr);
    data_ptr += 4;
    cursor += buffer_read((uint8_t*)header.format, sizeof(char), 4, data_ptr);// "WAVE"
    data_ptr += 4;

    if (cursor < 9 || !ChunkIDMatches(header.file_id, "RIFF") || !ChunkIDMatches(header.format, "WAVE")) {
        // TODO: Use error code
        return -1;
    }

    while (true) { // "fmt "
        buffer_read((uint8_t*)header.subchunk_id, sizeof(char), 4, data_ptr);
        data_ptr += 4;
        buffer_read((uint8_t*)&header.subchunk_size, sizeof(uint32_t), 1, data_ptr);
        data_ptr += 4;
        if (ChunkIDMatches(header.subchunk_id, "fmt ")) {
            break;
        }
        else {
            data_ptr += header.subchunk_size;// skip this subchunk by moving pointer
        }
    }

    // fmt Subchunk
    cursor = buffer_read((uint8_t*)&header.audio_format, sizeof(uint16_t), 1, data_ptr);
    data_ptr += 2;
    cursor += buffer_read((uint8_t*)&header.channel_count, sizeof(uint16_t), 1, data_ptr);
    data_ptr += 2;
    cursor += buffer_read((uint8_t*)&header.sample_rate, sizeof(uint32_t), 1, data_ptr);
    data_ptr += 4;
    cursor += buffer_read((uint8_t*)&header.byte_rate, sizeof(uint32_t), 1, data_ptr);
    data_ptr += 4;
    cursor += buffer_read((uint8_t*)&header.block_align, sizeof(uint16_t), 1, data_ptr);
    data_ptr += 2;
    cursor += buffer_read((uint8_t*)&header.bits_per_sample, sizeof(uint16_t), 1, data_ptr);
    data_ptr += 2;

    if (cursor != 6) {
        // TODO: Use error code
        return -1;
    }

    while (true) { // "data"
        buffer_read((uint8_t*)header.data_id, sizeof(char), 4, data_ptr);
        data_ptr += 4;
        buffer_read((uint8_t*)&header.data_size, sizeof(uint32_t), 1, data_ptr);
        data_ptr += 4;
        if (ChunkIDMatches(header.data_id, "data")) {
            break;
        }
        else {
            data_ptr += header.data_size;// skip this subchunk by moving pointer
        }
    }

    output->header = header;
    output->data_offset = data_ptr - data;
    output->byte_per_sample = header.bits_per_sample / 8;
    output->sample_format = WavGetSampleFormat(header);

    output->sample_count = output->header.data_size / output->byte_per_sample;
    output->buffer.start = (uint8_t*)data;
    output->buffer.data = (float*)data_ptr;
    output->buffer.data_size = header.data_size + 8;

    return 0;
}// Loads in wav file in memory and convert to 32bit float format

 // Modifies WAVFile properties to match
int WavLoadBuffer(WAVFile* output, FILE* fp) {
    if (output->data_offset == 0) {
        return -1;
    }
    //fseek(fp, 0, SEEK_END); // Move to end of file
    size_t file_size = 0; // ftell(fp);

    switch (output->sample_format) {
    case WAV_32_FLOAT: {
        file_size = sizeof(output->header) + sizeof(float) * output->sample_count;

        output->buffer.start = (uint8_t*)malloc(file_size);
        // Places new header at start;
        memcpy(output->buffer.start, &output->header, sizeof(output->header));
        break;
    }
    case WAV_S32_PCM: {
        file_size = sizeof(output->header) + sizeof(float) * output->sample_count;

        output->buffer.start = (uint8_t*)malloc(file_size);
        // Places new header at start;
        memcpy(output->buffer.start, &output->header, sizeof(output->header));
        break;
    }
    case WAV_S24_PCM: {
        file_size = sizeof(output->header) + sizeof(float) * output->sample_count;

        output->buffer.start = (uint8_t*)malloc(file_size);
        // Places new header at start;
        memcpy(output->buffer.start, &output->header, sizeof(output->header));
        break;
    }
    case WAV_S16_PCM: {
        file_size = sizeof(output->header) + sizeof(float) * output->sample_count;

        output->buffer.start = (uint8_t*)malloc(file_size);
        // Places new header at start;
        memcpy(output->buffer.start, &output->header, sizeof(output->header));
        break;
    }
        // TODO: load and convert other formats.
    }

    if (output->buffer.start == NULL) {
        // Alloc failed
        return -1;
    }

    fseek(fp, output->data_offset, SEEK_SET);
    output->buffer.data = (float*)(output->buffer.start + sizeof(output->header));

    switch (output->sample_format) {
    case WAV_32_FLOAT: {
        // TODO: read only sample part
        float sample;
        size_t cursor;
        for (int i = 0; i < output->sample_count; i++) {
            cursor = fread(&sample, sizeof(float), 1, fp);
            if (cursor == 0) {
                free(output->buffer.start);
                return -1;
            }
            output->buffer.data[i] = sample;
        }
        break;
    }
    case WAV_S32_PCM: {
        float scale = 1. / ((double)(1 << 31));
        long input;
        size_t cursor;
        for (int i = 0; i < output->sample_count; i++) {
            cursor = fread(&input, sizeof(long), 1, fp);
            if (cursor == 0) {
                free(output->buffer.start);
                return -1;
            }
            float sample = input * scale;
            output->buffer.data[i] = sample;
        }
        break;
    }
    case WAV_S24_PCM: {
        float scale = 1.f / ((double)(1 << 23));
        char buffer[3];
        size_t cursor;
        for (int i = 0; i < output->sample_count; i++) {
            cursor = fread(&buffer, sizeof(char), 3, fp);
            if (cursor != 3) {
                free(output->buffer.start);
                return -1;
            }
            float sample = convert24to32((uint8_t*)buffer) * scale;
            output->buffer.data[i] = sample;
        }
        break;
    }
    case WAV_S16_PCM: {
        float scale = 1.f / ((double)(1 << 15));
        short input;
        size_t cursor;
        for (int i = 0; i < output->sample_count; i++) {
            cursor = fread(&input, sizeof(short), 1, fp);
            if (cursor == 0) {
                free(output->buffer.start);
                return -1;
            }
            float sample = input * scale;
            output->buffer.data[i] = sample;
        }
        break;
    }
    }
    // All formats are converted to 32bit float
    output->sample_format = WAV_32_FLOAT;
    output->buffer.data_size = output->sample_count * sizeof(float);
    output->data_offset = sizeof(output->header); // After file cursor is set
    output->header.audio_format = 3;
    output->byte_per_sample = 4;

    return 0;
}

// Loads in wav file in memory and convert to 32bit float format
// Modifies WAVFile properties to match
int WavConvertBufferSamples(float* target, WAVFile* source){
  if (source->data_offset == 0 || source->buffer.data == NULL){
    return -1;
  }

  switch (source->sample_format){
    case WAV_32_FLOAT:{
      // TODO: read only sample part
      float sample;
      size_t cursor;
      float* pSource = (float*)source->buffer.data;
      for (int i = 0; i < source->sample_count; i++){
        cursor = buffer_read((uint8_t*)&sample, sizeof(float), 1, (uint8_t*)pSource++);
        if (cursor == 0){
          return -1;
        }
        target[i] = sample;
      }
      break;
    }
    case WAV_S32_PCM:{
      float scale = 1. / ((double)(1 << 31));
      long input;
      size_t cursor;
      long* pSource = (long*)source->buffer.data;
      for (int i = 0; i < source->sample_count; i++){
        cursor = buffer_read((uint8_t*)&input, sizeof(long), 1, (uint8_t*)pSource++);
        if (cursor == 0){
          return -1;
        }
        float sample = input * scale;
        target[i] = sample;
      }
      break;
    }
    case WAV_S24_PCM:{
      float scale = 1.f / ((double)(1 << 23));
      char buffer[3];
      size_t cursor;
      char* pSource = (char*)source->buffer.data;
      for (int i = 0; i < source->sample_count; i++){
        cursor = buffer_read((uint8_t*)&buffer, sizeof(char), 3, (uint8_t*)pSource);
        if (cursor != 3){
          return -1;
        }
        pSource += 3;
        float sample = convert24to32((uint8_t*)buffer) * scale;
        target[i] = sample;
      }
      break;
    }
    case WAV_S16_PCM:{
      float scale = 1.f / ((double)(1 << 15));
      short input;
      size_t cursor;
      short* pSource = (short*)source->buffer.data;
      for (int i = 0; i < source->sample_count; i++){
        cursor = buffer_read((uint8_t*)&input, sizeof(short), 1, (uint8_t*)pSource++);
        if (cursor == 0){
          return -1;
        }
        float sample = input * scale;
        target[i] = sample;
      }
      break;
    }
  }
  return 0;
}

// Supports only 32bit float samples
int WavWriteToFile(FILE* fp, WAVHeader header, float* sample_buffer, uint32_t sample_count) {
    if (fp == NULL) return 1;

    // set it as 32bit float
    header.audio_format = 3;
    header.bits_per_sample = 32;

    header.data_size = sample_count * sizeof(float);
    header.file_size = sizeof(WAVHeader) - 8 + header.data_size; // skips 8 bytes = RIFF + uint32 filesize?
    fwrite((void*)&header, sizeof(header), 1, fp);

    float* samples = sample_buffer;
    for (int i = 0; i < sample_count; i++) {
        float sample = samples[i];
        fwrite((void*)&sample, sizeof(sample), 1, fp);
    }
    return 0;
}

void WavCleanup(WAVFile *wav_file){
    free(wav_file->buffer.start);
    wav_file->buffer.start = NULL;
    wav_file->buffer.data = NULL;
}

#endif // WAV_H