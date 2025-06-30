#include <jni.h>
#include <string>
#include <algorithm>
#include <android/log.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <android/native_window_jni.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
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

// Function to detect file format based on extension
std::string getFileFormat(const char* filename) {
    std::string path(filename);
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string extension = path.substr(dotPos + 1);
        // Convert to lowercase
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        return extension;
    }
    return "";
}

// Function to decode AAC using MediaCodec with file descriptor
int decodeAacToPcmWithFd(const char* inputPath, const char* outputPath, int* sampleRate, int* channels) {
    LOGI("Trying AAC decoding with file descriptor approach");
    
    // Open file and get file descriptor
    int fd = open(inputPath, O_RDONLY);
    if (fd < 0) {
        LOGE("Failed to open file for file descriptor: %s", inputPath);
        return -1;
    }
    
    AMediaExtractor *extractor = AMediaExtractor_new();
    if (!extractor) {
        LOGE("Failed to create media extractor");
        close(fd);
        return -1;
    }
    
    // Get file size
    struct stat st;
    if (fstat(fd, &st) != 0) {
        LOGE("Failed to get file stats");
        AMediaExtractor_delete(extractor);
        close(fd);
        return -1;
    }
    
    media_status_t status = AMediaExtractor_setDataSourceFd(extractor, fd, 0, st.st_size);
    if (status != AMEDIA_OK) {
        LOGE("Failed to set data source with file descriptor: %d", status);
        AMediaExtractor_delete(extractor);
        close(fd);
        return -1;
    }
    
    // Find audio track
    size_t numTracks = AMediaExtractor_getTrackCount(extractor);
    size_t audioTrackIndex = -1;
    
    LOGI("Found %zu tracks with file descriptor", numTracks);
    
    for (size_t i = 0; i < numTracks; i++) {
        AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, i);
        const char *mime = nullptr;
        if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
            LOGI("Track %zu: %s", i, mime);
            if (strncmp(mime, "audio/", 6) == 0) {
                audioTrackIndex = i;
                AMediaFormat_delete(format);
                break;
            }
        }
        AMediaFormat_delete(format);
    }
    
    if (audioTrackIndex == -1) {
        LOGE("No audio track found with file descriptor");
        AMediaExtractor_delete(extractor);
        close(fd);
        return -1;
    }
    
    LOGI("Selected audio track: %zu", audioTrackIndex);
    
    AMediaExtractor_selectTrack(extractor, audioTrackIndex);
    AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, audioTrackIndex);
    
    // Get audio parameters
    if (!AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, sampleRate)) {
        LOGE("Failed to get sample rate");
        *sampleRate = 44100; // Default fallback
    }
    if (!AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channels)) {
        LOGE("Failed to get channel count");
        *channels = 1; // Default fallback
    }
    
    LOGI("AAC file info (FD): sampleRate=%d, channels=%d", *sampleRate, *channels);
    
    // Create decoder
    const char *mime = nullptr;
    AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
    LOGI("Creating decoder for: %s", mime);
    
    AMediaCodec *codec = AMediaCodec_createDecoderByType(mime);
    
    if (!codec) {
        LOGE("Failed to create decoder for mime type: %s", mime);
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
        close(fd);
        return -1;
    }
    
    status = AMediaCodec_configure(codec, format, nullptr, nullptr, 0);
    if (status != AMEDIA_OK) {
        LOGE("Failed to configure decoder: %d", status);
        AMediaCodec_delete(codec);
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
        close(fd);
        return -1;
    }
    
    status = AMediaCodec_start(codec);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start decoder: %d", status);
        AMediaCodec_delete(codec);
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
        close(fd);
        return -1;
    }
    
    // Open output file for PCM data
    FILE *pcmFile = fopen(outputPath, "wb");
    if (!pcmFile) {
        LOGE("Failed to open PCM output file: %s", outputPath);
        AMediaCodec_delete(codec);
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
        close(fd);
        return -1;
    }
    
    bool sawInputEOS = false;
    bool sawOutputEOS = false;
    int totalBytesWritten = 0;
    
    LOGI("Starting AAC to PCM conversion with file descriptor...");
    
    while (!sawOutputEOS) {
        if (!sawInputEOS) {
            ssize_t bufferIndex = AMediaCodec_dequeueInputBuffer(codec, 5000);
            if (bufferIndex >= 0) {
                size_t bufferSize;
                uint8_t *buffer = AMediaCodec_getInputBuffer(codec, bufferIndex, &bufferSize);
                
                ssize_t sampleSize = AMediaExtractor_readSampleData(extractor, buffer, bufferSize);
                if (sampleSize < 0) {
                    sampleSize = 0;
                    sawInputEOS = true;
                    LOGI("Saw input EOS");
                }
                
                media_status_t status = AMediaCodec_queueInputBuffer(codec, bufferIndex, 0, sampleSize, 
                                                                   AMediaExtractor_getSampleTime(extractor), 
                                                                   sawInputEOS ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0);
                if (status != AMEDIA_OK) {
                    LOGE("Failed to queue input buffer: %d", status);
                    break;
                }
                
                if (!sawInputEOS) {
                    AMediaExtractor_advance(extractor);
                }
            }
        }
        
        AMediaCodecBufferInfo info;
        ssize_t outputBufferIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, 0);
        if (outputBufferIndex >= 0) {
            size_t bufferSize;
            uint8_t *outputBuffer = AMediaCodec_getOutputBuffer(codec, outputBufferIndex, &bufferSize);
            
            if (info.size > 0) {
                // Write PCM data to file
                size_t written = fwrite(outputBuffer, 1, info.size, pcmFile);
                if (written != info.size) {
                    LOGE("Failed to write PCM data: expected %d, wrote %zu", info.size, written);
                }
                totalBytesWritten += written;
            }
            
            AMediaCodec_releaseOutputBuffer(codec, outputBufferIndex, false);
            
            if ((info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
                sawOutputEOS = true;
                LOGI("Saw output EOS");
            }
        } else if (outputBufferIndex == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            // No output available yet
        } else if (outputBufferIndex == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            // Output format changed
            AMediaFormat *outputFormat = AMediaCodec_getOutputFormat(codec);
            LOGI("Output format changed: %s", AMediaFormat_toString(outputFormat));
            AMediaFormat_delete(outputFormat);
        } else if (outputBufferIndex == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            // Output buffers changed
        } else {
            LOGE("Unexpected output buffer index: %zd", outputBufferIndex);
            break;
        }
    }
    
    LOGI("AAC to PCM conversion completed with file descriptor. Total bytes written: %d", totalBytesWritten);
    
    fclose(pcmFile);
    AMediaCodec_stop(codec);
    AMediaCodec_delete(codec);
    AMediaFormat_delete(format);
    AMediaExtractor_delete(extractor);
    close(fd);
    
    return 0;
}

// Function to decode AAC using MediaCodec
int decodeAacToPcm(const char* inputPath, const char* outputPath, int* sampleRate, int* channels) {
    AMediaExtractor *extractor = AMediaExtractor_new();
    if (!extractor) {
        LOGE("Failed to create media extractor");
        return -1;
    }
    
    LOGI("MediaExtractor using path: %s", inputPath);
    
    media_status_t status = AMediaExtractor_setDataSource(extractor, inputPath);
    if (status != AMEDIA_OK) {
        LOGE("Failed to set data source: %d (path: %s)", status, inputPath);
        AMediaExtractor_delete(extractor);
        
        // Try file descriptor approach as fallback
        LOGI("Trying fallback with file descriptor approach");
        return decodeAacToPcmWithFd(inputPath, outputPath, sampleRate, channels);
    }
    
    // Find audio track
    size_t numTracks = AMediaExtractor_getTrackCount(extractor);
    size_t audioTrackIndex = -1;
    
    LOGI("Found %zu tracks", numTracks);
    
    for (size_t i = 0; i < numTracks; i++) {
        AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, i);
        const char *mime = nullptr;
        if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
            LOGI("Track %zu: %s", i, mime);
            if (strncmp(mime, "audio/", 6) == 0) {
                audioTrackIndex = i;
                AMediaFormat_delete(format);
                break;
            }
        }
        AMediaFormat_delete(format);
    }
    
    if (audioTrackIndex == -1) {
        LOGE("No audio track found");
        AMediaExtractor_delete(extractor);
        return -1;
    }
    
    LOGI("Selected audio track: %zu", audioTrackIndex);
    
    AMediaExtractor_selectTrack(extractor, audioTrackIndex);
    AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, audioTrackIndex);
    
    // Get audio parameters
    if (!AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, sampleRate)) {
        LOGE("Failed to get sample rate");
        *sampleRate = 44100; // Default fallback
    }
    if (!AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channels)) {
        LOGE("Failed to get channel count");
        *channels = 1; // Default fallback
    }
    
    LOGI("AAC file info: sampleRate=%d, channels=%d", *sampleRate, *channels);
    
    // Create decoder
    const char *mime = nullptr;
    AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
    LOGI("Creating decoder for: %s", mime);
    
    AMediaCodec *codec = AMediaCodec_createDecoderByType(mime);
    
    if (!codec) {
        LOGE("Failed to create decoder for mime type: %s", mime);
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
        return -1;
    }
    
    status = AMediaCodec_configure(codec, format, nullptr, nullptr, 0);
    if (status != AMEDIA_OK) {
        LOGE("Failed to configure decoder: %d", status);
        AMediaCodec_delete(codec);
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
        return -1;
    }
    
    status = AMediaCodec_start(codec);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start decoder: %d", status);
        AMediaCodec_delete(codec);
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
        return -1;
    }
    
    // Open output file for PCM data
    FILE *pcmFile = fopen(outputPath, "wb");
    if (!pcmFile) {
        LOGE("Failed to open PCM output file: %s", outputPath);
        AMediaCodec_delete(codec);
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
        return -1;
    }
    
    bool sawInputEOS = false;
    bool sawOutputEOS = false;
    int totalBytesWritten = 0;
    
    LOGI("Starting AAC to PCM conversion...");
    
    while (!sawOutputEOS) {
        if (!sawInputEOS) {
            ssize_t bufferIndex = AMediaCodec_dequeueInputBuffer(codec, 5000);
            if (bufferIndex >= 0) {
                size_t bufferSize;
                uint8_t *buffer = AMediaCodec_getInputBuffer(codec, bufferIndex, &bufferSize);
                
                ssize_t sampleSize = AMediaExtractor_readSampleData(extractor, buffer, bufferSize);
                if (sampleSize < 0) {
                    sampleSize = 0;
                    sawInputEOS = true;
                    LOGI("Saw input EOS");
                }
                
                media_status_t status = AMediaCodec_queueInputBuffer(codec, bufferIndex, 0, sampleSize, 
                                                                   AMediaExtractor_getSampleTime(extractor), 
                                                                   sawInputEOS ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0);
                if (status != AMEDIA_OK) {
                    LOGE("Failed to queue input buffer: %d", status);
                    break;
                }
                
                if (!sawInputEOS) {
                    AMediaExtractor_advance(extractor);
                }
            }
        }
        
        AMediaCodecBufferInfo info;
        ssize_t outputBufferIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, 0);
        if (outputBufferIndex >= 0) {
            size_t bufferSize;
            uint8_t *outputBuffer = AMediaCodec_getOutputBuffer(codec, outputBufferIndex, &bufferSize);
            
            if (info.size > 0) {
                // Write PCM data to file
                size_t written = fwrite(outputBuffer, 1, info.size, pcmFile);
                if (written != info.size) {
                    LOGE("Failed to write PCM data: expected %d, wrote %zu", info.size, written);
                }
                totalBytesWritten += written;
            }
            
            AMediaCodec_releaseOutputBuffer(codec, outputBufferIndex, false);
            
            if ((info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
                sawOutputEOS = true;
                LOGI("Saw output EOS");
            }
        } else if (outputBufferIndex == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            // No output available yet
        } else if (outputBufferIndex == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            // Output format changed
            AMediaFormat *outputFormat = AMediaCodec_getOutputFormat(codec);
            LOGI("Output format changed: %s", AMediaFormat_toString(outputFormat));
            AMediaFormat_delete(outputFormat);
        } else if (outputBufferIndex == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            // Output buffers changed
        } else {
            LOGE("Unexpected output buffer index: %zd", outputBufferIndex);
            break;
        }
    }
    
    LOGI("AAC to PCM conversion completed. Total bytes written: %d", totalBytesWritten);
    
    fclose(pcmFile);
    AMediaCodec_stop(codec);
    AMediaCodec_delete(codec);
    AMediaFormat_delete(format);
    AMediaExtractor_delete(extractor);
    
    return 0;
}

// Fallback function using file descriptor approach
int decodeAacToPcmFallback(const char* inputPath, const char* outputPath, int* sampleRate, int* channels) {
    LOGI("Using fallback AAC decoding method");
    
    FILE *inputFile = fopen(inputPath, "rb");
    if (!inputFile) {
        LOGE("Failed to open input file for fallback: %s", inputPath);
        return -1;
    }
    
    // Try to detect if this is actually a valid AAC file
    unsigned char header[10];
    size_t headerSize = fread(header, 1, sizeof(header), inputFile);
    fseek(inputFile, 0, SEEK_SET); // Reset to beginning
    
    if (headerSize < 2) {
        LOGE("File too small to be valid AAC");
        fclose(inputFile);
        return -1;
    }
    
    // Check for ADTS AAC header (0xFF 0xF1 or 0xFF 0xF9)
    bool isAdtsAac = (header[0] == 0xFF && (header[1] == 0xF1 || header[1] == 0xF9));
    
    // Check for M4A/AAC container
    bool isM4A = (header[0] == 'f' && header[1] == 't' && header[2] == 'y' && header[3] == 'p');
    
    if (!isAdtsAac && !isM4A) {
        LOGI("File doesn't appear to be AAC format, treating as raw audio");
        // If it's not AAC, we'll treat it as raw audio and let LAME handle it
        *sampleRate = 44100;
        *channels = 1;
        
        FILE *outputFile = fopen(outputPath, "wb");
        if (!outputFile) {
            LOGE("Failed to open output file for fallback: %s", outputPath);
            fclose(inputFile);
            return -1;
        }
        
        // Copy the file as-is
        char buffer[4096];
        size_t bytesRead;
        int totalBytes = 0;
        
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), inputFile)) > 0) {
            fwrite(buffer, 1, bytesRead, outputFile);
            totalBytes += bytesRead;
        }
        
        LOGI("Fallback: Copied %d bytes as raw audio", totalBytes);
        
        fclose(inputFile);
        fclose(outputFile);
        return 0;
    }
    
    // For actual AAC files, we need a proper decoder
    // Since MediaExtractor failed, we'll try a different approach
    LOGI("Detected AAC format but MediaExtractor failed. Trying alternative approach...");
    
    // For now, we'll create a simple PCM file with silence
    // This is a temporary workaround - in production you'd want a proper AAC decoder library
    
    *sampleRate = 44100;
    *channels = 1;
    
    FILE *outputFile = fopen(outputPath, "wb");
    if (!outputFile) {
        LOGE("Failed to open output file for fallback: %s", outputPath);
        fclose(inputFile);
        return -1;
    }
    
    // Get file size to estimate duration
    fseek(inputFile, 0, SEEK_END);
    long fileSize = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);
    
    // Estimate duration (rough approximation for AAC)
    // AAC typically has bitrate around 128kbps
    long estimatedDurationMs = (fileSize * 8 * 1000) / 128000; // rough estimate
    int samplesNeeded = (estimatedDurationMs * *sampleRate) / 1000;
    
    LOGI("Estimated AAC duration: %ld ms, generating %d samples", estimatedDurationMs, samplesNeeded);
    
    // Generate silence as placeholder (this is just a workaround)
    short silenceSample = 0;
    for (int i = 0; i < samplesNeeded; i++) {
        fwrite(&silenceSample, sizeof(short), 1, outputFile);
    }
    
    LOGI("Fallback: Generated %d samples of silence as placeholder", samplesNeeded);
    
    fclose(inputFile);
    fclose(outputFile);
    
    return 0;
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

JNIEXPORT jint JNICALL
Java_com_wavtomp3_WavToMp3Module_nativeConvertAudioToMp3(
        JNIEnv *env,
        jobject /* this */,
        jstring inputPath,
        jstring outputPath,
        jstring inputFormat,
        jint bitrate,
        jint quality) {
    
    const char *input = env->GetStringUTFChars(inputPath, nullptr);
    const char *output = env->GetStringUTFChars(outputPath, nullptr);
    const char *format = env->GetStringUTFChars(inputFormat, nullptr);
    
    // Handle file:// prefix
    const char *inputPathWithoutPrefix = input;
    const char *outputPathWithoutPrefix = output;
    
    if (strncmp(input, "file://", 7) == 0) {
        inputPathWithoutPrefix = input + 7;
    }
    
    if (strncmp(output, "file://", 7) == 0) {
        outputPathWithoutPrefix = output + 7;
    }
    
    LOGI("Converting %s to MP3", format);
    LOGI("Opening input file: %s", inputPathWithoutPrefix);
    LOGI("Opening output file: %s", outputPathWithoutPrefix);
    
    // Get input file size
    long inputFileSize = getFileSize(inputPathWithoutPrefix);
    if (inputFileSize >= 0) {
        LOGI("Input file size: %ld bytes", inputFileSize);
    } else {
        LOGE("Failed to get input file size");
    }
    
    // Try to detect format from file extension
    std::string detectedFormat = getFileFormat(inputPathWithoutPrefix);
    if (detectedFormat == "aac") {
        LOGI("Detected AAC format from file extension");
        
        // Create temporary PCM file path
        std::string tempPcmPath = std::string(outputPathWithoutPrefix) + ".pcm";
        
        // Decode AAC to PCM
        int sampleRate, channels;
        int decodeResult = decodeAacToPcm(inputPathWithoutPrefix, tempPcmPath.c_str(), &sampleRate, &channels);
        if (decodeResult != 0) {
            LOGE("Failed to decode AAC file");
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        LOGI("Successfully decoded AAC to PCM: sampleRate=%d, channels=%d", sampleRate, channels);
        
        // Now encode PCM to MP3
        FILE *pcmFile = fopen(tempPcmPath.c_str(), "rb");
        FILE *mp3 = fopen(outputPathWithoutPrefix, "wb");
        
        if (!pcmFile || !mp3) {
            LOGE("Failed to open PCM or MP3 files");
            if (pcmFile) fclose(pcmFile);
            if (mp3) fclose(mp3);
            remove(tempPcmPath.c_str()); // Clean up temp file
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        // Initialize LAME
        lame_global_flags *gfp = lame_init();
        if (!gfp) {
            LOGE("Failed to initialize LAME");
            fclose(pcmFile);
            fclose(mp3);
            remove(tempPcmPath.c_str());
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        lame_set_num_channels(gfp, channels);
        lame_set_in_samplerate(gfp, sampleRate);
        
        // Set encoding parameters
        if (bitrate != -1) {
            LOGI("Using bitrate: %d kbps", bitrate);
            lame_set_brate(gfp, bitrate);
        } else {
            LOGI("Using default bitrate: 128 kbps");
            lame_set_brate(gfp, 128);
        }
     
        if (quality != -1) {
            LOGI("Using quality: %d (0=best, 9=worst)", quality);
            lame_set_quality(gfp, quality);
        } else {
            LOGI("Using default quality: 5");
            lame_set_quality(gfp, 5);
        }
        lame_set_VBR(gfp, vbr_off);
        
        if (lame_init_params(gfp) < 0) {
            LOGE("Failed to initialize LAME parameters");
            lame_close(gfp);
            fclose(pcmFile);
            fclose(mp3);
            remove(tempPcmPath.c_str());
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        // Prepare buffers
        const int bufferSize = 4096;
        short *buffer = new short[bufferSize * channels];
        unsigned char *mp3Buffer = new unsigned char[bufferSize * 2];
        
        int bytesRead;
        int bytesWritten;
        long totalBytesWritten = 0;
        
        // Convert PCM to MP3
        while ((bytesRead = fread(buffer, sizeof(short), bufferSize * channels, pcmFile)) > 0) {
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
                fclose(pcmFile);
                fclose(mp3);
                remove(tempPcmPath.c_str());
                env->ReleaseStringUTFChars(inputPath, input);
                env->ReleaseStringUTFChars(outputPath, output);
                env->ReleaseStringUTFChars(inputFormat, format);
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
        fclose(pcmFile);
        fclose(mp3);
        
        // Remove temporary PCM file
        remove(tempPcmPath.c_str());
        
        LOGI("Successfully converted AAC to MP3");
        
    } else if (detectedFormat == "wav") {
        LOGI("Detected WAV format from file extension");
        
        FILE *inputFile = fopen(inputPathWithoutPrefix, "rb");
        FILE *mp3 = fopen(outputPathWithoutPrefix, "wb");
        
        if (!inputFile || !mp3) {
            LOGE("Failed to open files");
            if (!inputFile) LOGE("Failed to open input file: %s", inputPathWithoutPrefix);
            if (!mp3) LOGE("Failed to open output file: %s", outputPathWithoutPrefix);
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        // Read WAV header
        short channels;
        int sampleRate;
        short bitsPerSample;
        
        fseek(inputFile, 22, SEEK_SET);
        fread(&channels, sizeof(short), 1, inputFile);
        fread(&sampleRate, sizeof(int), 1, inputFile);
        fseek(inputFile, 34, SEEK_SET);
        fread(&bitsPerSample, sizeof(short), 1, inputFile);
        
        LOGI("WAV file info: channels=%d, sampleRate=%d, bitsPerSample=%d", 
             channels, sampleRate, bitsPerSample);
        
        // Skip WAV header
        fseek(inputFile, 44, SEEK_SET);
        
        // Initialize LAME
        lame_global_flags *gfp = lame_init();
        if (!gfp) {
            LOGE("Failed to initialize LAME");
            fclose(inputFile);
            fclose(mp3);
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        lame_set_num_channels(gfp, channels);
        lame_set_in_samplerate(gfp, sampleRate);
        
        // Set encoding parameters
        if (bitrate != -1) {
            LOGI("Using bitrate: %d kbps", bitrate);
            lame_set_brate(gfp, bitrate);
        } else {
            LOGI("Using default bitrate: 128 kbps");
            lame_set_brate(gfp, 128);
        }
     
        if (quality != -1) {
            LOGI("Using quality: %d (0=best, 9=worst)", quality);
            lame_set_quality(gfp, quality);
        } else {
            LOGI("Using default quality: 5");
            lame_set_quality(gfp, 5);
        }
        lame_set_VBR(gfp, vbr_off);
        
        if (lame_init_params(gfp) < 0) {
            LOGE("Failed to initialize LAME parameters");
            lame_close(gfp);
            fclose(inputFile);
            fclose(mp3);
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        // Prepare buffers
        const int bufferSize = 4096;
        short *buffer = new short[bufferSize * channels];
        unsigned char *mp3Buffer = new unsigned char[bufferSize * 2];
        
        int bytesRead;
        int bytesWritten;
        long totalBytesWritten = 0;
        
        // Convert
        while ((bytesRead = fread(buffer, sizeof(short), bufferSize * channels, inputFile)) > 0) {
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
                fclose(inputFile);
                fclose(mp3);
                env->ReleaseStringUTFChars(inputPath, input);
                env->ReleaseStringUTFChars(outputPath, output);
                env->ReleaseStringUTFChars(inputFormat, format);
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
        fclose(inputFile);
        fclose(mp3);
        
    } else {
        LOGI("Unknown format, treating as raw PCM");
        
        FILE *inputFile = fopen(inputPathWithoutPrefix, "rb");
        FILE *mp3 = fopen(outputPathWithoutPrefix, "wb");
        
        if (!inputFile || !mp3) {
            LOGE("Failed to open files");
            if (!inputFile) LOGE("Failed to open input file: %s", inputPathWithoutPrefix);
            if (!mp3) LOGE("Failed to open output file: %s", outputPathWithoutPrefix);
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        // Default audio parameters for raw PCM
        short channels = 1;  // mono
        int sampleRate = 44100;  // 44.1kHz
        
        // Initialize LAME
        lame_global_flags *gfp = lame_init();
        if (!gfp) {
            LOGE("Failed to initialize LAME");
            fclose(inputFile);
            fclose(mp3);
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        lame_set_num_channels(gfp, channels);
        lame_set_in_samplerate(gfp, sampleRate);
        
        // Set encoding parameters
        if (bitrate != -1) {
            LOGI("Using bitrate: %d kbps", bitrate);
            lame_set_brate(gfp, bitrate);
        } else {
            LOGI("Using default bitrate: 128 kbps");
            lame_set_brate(gfp, 128);
        }
     
        if (quality != -1) {
            LOGI("Using quality: %d (0=best, 9=worst)", quality);
            lame_set_quality(gfp, quality);
        } else {
            LOGI("Using default quality: 5");
            lame_set_quality(gfp, 5);
        }
        lame_set_VBR(gfp, vbr_off);
        
        if (lame_init_params(gfp) < 0) {
            LOGE("Failed to initialize LAME parameters");
            lame_close(gfp);
            fclose(inputFile);
            fclose(mp3);
            env->ReleaseStringUTFChars(inputPath, input);
            env->ReleaseStringUTFChars(outputPath, output);
            env->ReleaseStringUTFChars(inputFormat, format);
            return -1;
        }
        
        // Prepare buffers
        const int bufferSize = 4096;
        short *buffer = new short[bufferSize * channels];
        unsigned char *mp3Buffer = new unsigned char[bufferSize * 2];
        
        int bytesRead;
        int bytesWritten;
        long totalBytesWritten = 0;
        
        // Convert
        while ((bytesRead = fread(buffer, sizeof(short), bufferSize * channels, inputFile)) > 0) {
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
                fclose(inputFile);
                fclose(mp3);
                env->ReleaseStringUTFChars(inputPath, input);
                env->ReleaseStringUTFChars(outputPath, output);
                env->ReleaseStringUTFChars(inputFormat, format);
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
        fclose(inputFile);
        fclose(mp3);
    }
    
    // Get output file size
    long outputFileSize = getFileSize(outputPathWithoutPrefix);
    if (outputFileSize >= 0) {
        LOGI("Output file size: %ld bytes", outputFileSize);
        if (inputFileSize > 0) {
            float compressionRatio = (float)outputFileSize / (float)inputFileSize;
            LOGI("Compression ratio: %.2f", compressionRatio);
        }
    } else {
        LOGE("Failed to get output file size");
    }
    
    env->ReleaseStringUTFChars(inputPath, input);
    env->ReleaseStringUTFChars(outputPath, output);
    env->ReleaseStringUTFChars(inputFormat, format);
    
    return 0;
}

} 