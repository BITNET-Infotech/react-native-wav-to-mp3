#include <jni.h>
#include <string>
#include <android/log.h>
#include <sys/stat.h>
#include "lame/lame.h"

#define LOG_TAG "WavToMp3"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Function to get file size
long getFileSize(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

extern "C" {

JNIEXPORT jint JNICALL
Java_com_wavtomp3_WavToMp3Module_nativeConvertWavToMp3(
        JNIEnv *env,
        jobject /* this */,
        jstring inputPath,
        jstring outputPath,
        jint bitrate,
        jint quality) {
    
    const char *input = env->GetStringUTFChars(inputPath, nullptr);
    const char *output = env->GetStringUTFChars(outputPath, nullptr);
    
    // Handle file:// prefix
    const char *inputPathWithoutPrefix = input;
    const char *outputPathWithoutPrefix = output;
    
    if (strncmp(input, "file://", 7) == 0) {
        inputPathWithoutPrefix = input + 7;
    }
    
    if (strncmp(output, "file://", 7) == 0) {
        outputPathWithoutPrefix = output + 7;
    }
    
    LOGI("Opening input file: %s", inputPathWithoutPrefix);
    LOGI("Opening output file: %s", outputPathWithoutPrefix);
    
    // Get input file size
    long inputFileSize = getFileSize(inputPathWithoutPrefix);
    if (inputFileSize >= 0) {
        LOGI("Input file size: %ld bytes", inputFileSize);
    } else {
        LOGE("Failed to get input file size");
    }
    
    FILE *wav = fopen(inputPathWithoutPrefix, "rb");
    FILE *mp3 = fopen(outputPathWithoutPrefix, "wb");
    
    if (!wav || !mp3) {
        LOGE("Failed to open files");
        if (!wav) LOGE("Failed to open input file: %s", inputPathWithoutPrefix);
        if (!mp3) LOGE("Failed to open output file: %s", outputPathWithoutPrefix);
        env->ReleaseStringUTFChars(inputPath, input);
        env->ReleaseStringUTFChars(outputPath, output);
        return -1;
    }
    
    // Read WAV header
    short channels;
    int sampleRate;
    short bitsPerSample;
    
    fseek(wav, 22, SEEK_SET);
    fread(&channels, sizeof(short), 1, wav);
    fread(&sampleRate, sizeof(int), 1, wav);
    fseek(wav, 34, SEEK_SET);
    fread(&bitsPerSample, sizeof(short), 1, wav);
    
    LOGI("WAV file info: channels=%d, sampleRate=%d, bitsPerSample=%d", 
         channels, sampleRate, bitsPerSample);
    
    // Initialize LAME
    lame_global_flags *gfp = lame_init();
    if (!gfp) {
        LOGE("Failed to initialize LAME");
        fclose(wav);
        fclose(mp3);
        env->ReleaseStringUTFChars(inputPath, input);
        env->ReleaseStringUTFChars(outputPath, output);
        return -1;
    }
    
    lame_set_num_channels(gfp, channels);
    lame_set_in_samplerate(gfp, sampleRate);
    
    // Set encoding parameters based on provided options
    if (bitrate != -1) {
        // If bitrate is provided, use it and disable VBR
        LOGI("Using bitrate: %d kbps", bitrate);
        lame_set_brate(gfp, bitrate);
    } else {
        // Default settings if no bitrate provided
        LOGI("Using default bitrate: 128 kbps");
        lame_set_brate(gfp, 128);
    }
 
    if (quality != -1) {
        // If quality is provided, use it and enable VBR
        LOGI("Using quality: %d (0=best, 9=worst)", quality);
        lame_set_quality(gfp, quality);
    } else {
        // Default settings if no options provided
        LOGI("Using default settings: bitrate=128kbps, quality=5");
        lame_set_quality(gfp, 5);
    }
    lame_set_VBR(gfp, vbr_off);
    
    if (lame_init_params(gfp) < 0) {
        LOGE("Failed to initialize LAME parameters");
        lame_close(gfp);
        fclose(wav);
        fclose(mp3);
        env->ReleaseStringUTFChars(inputPath, input);
        env->ReleaseStringUTFChars(outputPath, output);
        return -1;
    }
    
    // Prepare buffers
    const int bufferSize = 4096;
    short *buffer = new short[bufferSize * channels];
    unsigned char *mp3Buffer = new unsigned char[bufferSize * 2];
    
    // Skip WAV header
    fseek(wav, 44, SEEK_SET);
    
    int bytesRead;
    int bytesWritten;
    long totalBytesWritten = 0;
    
    // Convert
    while ((bytesRead = fread(buffer, sizeof(short), bufferSize * channels, wav)) > 0) {
        if (channels == 1) {
            bytesWritten = lame_encode_buffer(gfp, buffer, nullptr, bytesRead, mp3Buffer, bufferSize * 2);
        } else {
            bytesWritten = lame_encode_buffer_interleaved(gfp, buffer, bytesRead / channels, mp3Buffer, bufferSize * 2);
        }
        
        if (bytesWritten < 0) {
            LOGE("Failed to encode buffer");
            delete[] buffer;
            delete[] mp3Buffer;
            lame_close(gfp);
            fclose(wav);
            fclose(mp3);
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            return -1;
        }
        
        fwrite(mp3Buffer, 1, bytesWritten, mp3);
        totalBytesWritten += bytesWritten;
    }
    
    // Flush
    bytesWritten = lame_encode_flush(gfp, mp3Buffer, bufferSize * 2);
    if (bytesWritten > 0) {
        fwrite(mp3Buffer, 1, bytesWritten, mp3);
        totalBytesWritten += bytesWritten;
    }
    
    // Cleanup
    delete[] buffer;
    delete[] mp3Buffer;
    lame_close(gfp);
    fclose(wav);
    fclose(mp3);
    
    // Get output file size
    long outputFileSize = getFileSize(outputPathWithoutPrefix);
    if (outputFileSize >= 0) {
        LOGI("Output file size: %ld bytes", outputFileSize);
        LOGI("Total bytes written: %ld bytes", totalBytesWritten);
        if (inputFileSize > 0) {
            float compressionRatio = (float)outputFileSize / (float)inputFileSize;
            LOGI("Compression ratio: %.2f", compressionRatio);
        }
    } else {
        LOGE("Failed to get output file size");
    }
    
    env->ReleaseStringUTFChars(inputPath, input);
    env->ReleaseStringUTFChars(outputPath, output);
    
    return 0;
}

} 