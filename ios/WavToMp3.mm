#import "WavToMp3.h"
#import <React/RCTLog.h>
#import <LAME/lame.h>

@implementation WavToMp3

RCT_EXPORT_MODULE();

- (NSArray<NSString *> *)supportedEvents {
    return @[@"onProgress"];
}

RCT_EXPORT_METHOD(convertWavToMp3:(NSString *)inputPath
                  outputPath:(NSString *)outputPath
                  options:(NSDictionary *)options
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {
    
    // Remove file:// prefix if present
    if ([inputPath hasPrefix:@"file://"]) {
        inputPath = [inputPath substringFromIndex:7];
    }
    if ([outputPath hasPrefix:@"file://"]) {
        outputPath = [outputPath substringFromIndex:7];
    }
    
    // Ensure output directory exists
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *outputDir = [outputPath stringByDeletingLastPathComponent];
    NSError *error = nil;
    
    if (![fileManager fileExistsAtPath:outputDir]) {
        [fileManager createDirectoryAtPath:outputDir withIntermediateDirectories:YES attributes:nil error:&error];
        if (error) {
            reject(@"DIRECTORY_ERROR", @"Failed to create output directory", error);
            return;
        }
    }
    
    // Log file paths and sizes
    NSDictionary *inputAttributes = [fileManager attributesOfItemAtPath:inputPath error:&error];
    if (error) {
        reject(@"FILE_ERROR", @"Failed to get input file attributes", error);
        return;
    }
    
    RCTLogInfo(@"Input path: %@", inputPath);
    RCTLogInfo(@"Input file size: %llu bytes", [inputAttributes fileSize]);
    RCTLogInfo(@"Output path: %@", outputPath);
    
    // Open files
    FILE *wav = fopen([inputPath UTF8String], "rb");
    FILE *mp3 = fopen([outputPath UTF8String], "wb");
    
    if (!wav || !mp3) {
        if (wav) fclose(wav);
        if (mp3) fclose(mp3);
        reject(@"FILE_ERROR", @"Failed to open input or output file", nil);
        return;
    }
    
    // Read WAV header
    short channels;
    int sampleRate;
    short bitsPerSample;
    
    // Read RIFF header
    char riffHeader[4];
    fread(riffHeader, 1, 4, wav);
    if (strncmp(riffHeader, "RIFF", 4) != 0) {
        fclose(wav);
        fclose(mp3);
        reject(@"WAV_ERROR", @"Not a valid WAV file (missing RIFF header)", nil);
        return;
    }
    
    // Skip file size
    fseek(wav, 4, SEEK_CUR);
    
    // Read WAVE identifier
    char waveHeader[4];
    fread(waveHeader, 1, 4, wav);
    if (strncmp(waveHeader, "WAVE", 4) != 0) {
        fclose(wav);
        fclose(mp3);
        reject(@"WAV_ERROR", @"Not a valid WAV file (missing WAVE identifier)", nil);
        return;
    }
    
    // Search for fmt chunk
    char chunkId[4];
    unsigned int chunkSize;
    bool fmtFound = false;
    
    while (!fmtFound && !feof(wav)) {
        if (fread(chunkId, 1, 4, wav) != 4) {
            break;
        }
        
        if (fread(&chunkSize, 4, 1, wav) != 1) {
            break;
        }
        
        if (strncmp(chunkId, "fmt ", 4) == 0) {
            fmtFound = true;
            break;
        } else {
            // Skip this chunk
            fseek(wav, chunkSize, SEEK_CUR);
        }
    }
    
    if (!fmtFound) {
        fclose(wav);
        fclose(mp3);
        reject(@"WAV_ERROR", @"fmt chunk not found in WAV file", nil);
        return;
    }
    
    // Read fmt chunk data
    short audioFormat;
    fread(&audioFormat, sizeof(short), 1, wav);
    fread(&channels, sizeof(short), 1, wav);
    fread(&sampleRate, sizeof(int), 1, wav);
    
    int byteRate;
    short blockAlign;
    fread(&byteRate, sizeof(int), 1, wav);
    fread(&blockAlign, sizeof(short), 1, wav);
    fread(&bitsPerSample, sizeof(short), 1, wav);
    
    // Skip any remaining fmt chunk data
    if (chunkSize > 16) {
        fseek(wav, chunkSize - 16, SEEK_CUR);
    }
    
    // Search for data chunk
    bool dataFound = false;
    while (!dataFound && !feof(wav)) {
        if (fread(chunkId, 1, 4, wav) != 4) {
            break;
        }
        
        if (fread(&chunkSize, 4, 1, wav) != 1) {
            break;
        }
        
        if (strncmp(chunkId, "data", 4) == 0) {
            dataFound = true;
            break;
        } else {
            // Skip this chunk
            fseek(wav, chunkSize, SEEK_CUR);
        }
    }
    
    if (!dataFound) {
        fclose(wav);
        fclose(mp3);
        reject(@"WAV_ERROR", @"data chunk not found in WAV file", nil);
        return;
    }
    
    // Now we're positioned at the start of audio data
    long dataStartPosition = ftell(wav);
    
    RCTLogInfo(@"WAV file info: channels=%d, sampleRate=%d, bitsPerSample=%d, audioFormat=%d", 
               channels, sampleRate, bitsPerSample, audioFormat);
    RCTLogInfo(@"Data chunk size: %u bytes", chunkSize);
    RCTLogInfo(@"Data starts at position: %ld", dataStartPosition);
    
    // Validate audio format
    if (audioFormat != 1) {
        fclose(wav);
        fclose(mp3);
        reject(@"WAV_ERROR", @"Unsupported audio format (only PCM supported)", nil);
        return;
    }
    
    if (bitsPerSample != 16) {
        fclose(wav);
        fclose(mp3);
        reject(@"WAV_ERROR", @"Unsupported bit depth (only 16-bit supported)", nil);
        return;
    }
    
    // Initialize LAME
    lame_global_flags *gfp = lame_init();
    if (!gfp) {
        fclose(wav);
        fclose(mp3);
        reject(@"LAME_ERROR", @"Failed to initialize LAME", nil);
        return;
    }
    
    // Apply options if provided
    if (options) {
        NSNumber *bitrate = options[@"bitrate"];
        NSNumber *quality = options[@"quality"];
        
        if (bitrate) {
            lame_set_brate(gfp, [bitrate intValue]);
            RCTLogInfo(@"Setting bitrate to: %d", [bitrate intValue]);
        } else {
            lame_set_brate(gfp, 32); // Default 32kbps for maximum compression
        }
        
        if (quality) {
            lame_set_quality(gfp, [quality intValue]);
            RCTLogInfo(@"Setting quality to: %d", [quality intValue]);
        } else {
            lame_set_quality(gfp, 7); // Default quality 7 (good compression, acceptable for speech)
        }
    } else {
        lame_set_brate(gfp, 32); // 32kbps for maximum compression
        lame_set_quality(gfp, 7); // 0=best, 9=worst - 7 is good for speech
    }
    
    lame_set_num_channels(gfp, channels);
    lame_set_in_samplerate(gfp, sampleRate);
    lame_set_VBR(gfp, vbr_off);
    
    // Set additional parameters optimized for maximum compression and speech recognition
    lame_set_compression_ratio(gfp, 11.025); // Good compression ratio
    lame_set_force_ms(gfp, 0); // Don't force mid/side encoding
    
    // Audio quality improvements - preserve original volume
    lame_set_scale(gfp, 1.0); // Preserve original volume
    lame_set_scale_left(gfp, 1.0); // Left channel scaling
    lame_set_scale_right(gfp, 1.0); // Right channel scaling
    
    // Speech optimization settings for maximum compression
    lame_set_lowpassfreq(gfp, 8000); // Low-pass filter at 8kHz (speech frequencies)
    lame_set_highpassfreq(gfp, 80); // High-pass filter at 80Hz (remove low noise)
    lame_set_strict_ISO(gfp, 0); // Don't strictly follow ISO (allows better compression)
    lame_set_ATHonly(gfp, 0); // Use full psychoacoustic model
    lame_set_ATHshort(gfp, 0); // Use long blocks for better compression
    lame_set_noATH(gfp, 0); // Use ATH (Absolute Threshold of Hearing)
    lame_set_quant_comp(gfp, 0); // No quantization compensation
    lame_set_quant_comp_short(gfp, 0); // No short block quantization compensation
    
    // Better encoding settings
    lame_set_emphasis(gfp, 0); // No emphasis
    lame_set_original(gfp, 1); // Mark as original
    lame_set_copyright(gfp, 0); // No copyright bit
    lame_set_extension(gfp, 0); // No extension
    
    // Speech-specific optimizations
    if (sampleRate > 16000) {
        // For higher sample rates, we can be more aggressive with compression
        lame_set_lowpassfreq(gfp, 8000); // Focus on speech frequencies
    }
    
    if (lame_init_params(gfp) < 0) {
        lame_close(gfp);
        fclose(wav);
        fclose(mp3);
        reject(@"LAME_ERROR", @"Failed to initialize LAME parameters", nil);
        return;
    }
    
    // Prepare buffers
    const int bufferSize = 4096;
    short *buffer = (short *)malloc(bufferSize * channels * sizeof(short));
    unsigned char *mp3Buffer = (unsigned char *)malloc(bufferSize * 2);
    
    if (!buffer || !mp3Buffer) {
        if (buffer) free(buffer);
        if (mp3Buffer) free(mp3Buffer);
        lame_close(gfp);
        fclose(wav);
        fclose(mp3);
        reject(@"MEMORY_ERROR", @"Failed to allocate memory", nil);
        return;
    }
    
    // Position at the start of audio data (we already found this position)
    fseek(wav, dataStartPosition, SEEK_SET);
    
    int bytesRead;
    int bytesWritten;
    long totalBytesWritten = 0;
    long totalBytes = [inputAttributes fileSize];
    long bytesProcessed = dataStartPosition; // Start from where data begins
    
    // Convert
    while ((bytesRead = fread(buffer, sizeof(short), bufferSize * channels, wav)) > 0) {
        int samplesRead = bytesRead / channels;
        
        // Ensure we have complete samples
        if (samplesRead <= 0) continue;
        
        if (channels == 1) {
            bytesWritten = lame_encode_buffer(gfp, buffer, NULL, samplesRead, mp3Buffer, bufferSize * 2);
        } else {
            bytesWritten = lame_encode_buffer_interleaved(gfp, buffer, samplesRead, mp3Buffer, bufferSize * 2);
        }
        
        if (bytesWritten < 0) {
            free(buffer);
            free(mp3Buffer);
            lame_close(gfp);
            fclose(wav);
            fclose(mp3);
            reject(@"ENCODE_ERROR", @"Failed to encode buffer", nil);
            return;
        }
        
        fwrite(mp3Buffer, 1, bytesWritten, mp3);
        totalBytesWritten += bytesWritten;
        bytesProcessed += bytesRead * sizeof(short);
        
        // Send progress event
        float progress = (float)bytesProcessed / (float)totalBytes;
        [self sendEventWithName:@"onProgress" body:@{@"progress": @(progress)}];
    }
    
    // Flush
    bytesWritten = lame_encode_flush(gfp, mp3Buffer, bufferSize * 2);
    if (bytesWritten > 0) {
        fwrite(mp3Buffer, 1, bytesWritten, mp3);
        totalBytesWritten += bytesWritten;
    }
    
    // Cleanup
    free(buffer);
    free(mp3Buffer);
    lame_close(gfp);
    fclose(wav);
    fclose(mp3);
    
    // Get output file size
    NSDictionary *outputAttributes = [fileManager attributesOfItemAtPath:outputPath error:&error];
    if (error) {
        reject(@"FILE_ERROR", @"Failed to get output file attributes", error);
        return;
    }
    
    RCTLogInfo(@"Output file size: %llu bytes", [outputAttributes fileSize]);
    RCTLogInfo(@"Total bytes written: %ld bytes", totalBytesWritten);
    
    if ([inputAttributes fileSize] > 0) {
        float compressionRatio = (float)[outputAttributes fileSize] / (float)[inputAttributes fileSize];
        RCTLogInfo(@"Compression ratio: %.2f", compressionRatio);
    }
    
    resolve(outputPath);
}

@end
