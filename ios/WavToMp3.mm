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
    
    fseek(wav, 22, SEEK_SET);
    fread(&channels, sizeof(short), 1, wav);
    fread(&sampleRate, sizeof(int), 1, wav);
    fseek(wav, 34, SEEK_SET);
    fread(&bitsPerSample, sizeof(short), 1, wav);
    
    RCTLogInfo(@"WAV file info: channels=%d, sampleRate=%d, bitsPerSample=%d", 
               channels, sampleRate, bitsPerSample);
    
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
            lame_set_brate(gfp, 128); // Default 128kbps
        }
        
        if (quality) {
            lame_set_quality(gfp, [quality intValue]);
            RCTLogInfo(@"Setting quality to: %d", [quality intValue]);
        } else {
            lame_set_quality(gfp, 5); // Default quality
        }
    } else {
        lame_set_brate(gfp, 128); // 128kbps
        lame_set_quality(gfp, 5); // 0=best, 9=worst
    }
    
    lame_set_num_channels(gfp, channels);
    lame_set_in_samplerate(gfp, sampleRate);
    lame_set_VBR(gfp, vbr_off);
    
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
    
    // Skip WAV header
    fseek(wav, 44, SEEK_SET);
    
    int bytesRead;
    int bytesWritten;
    long totalBytesWritten = 0;
    long totalBytes = [inputAttributes fileSize];
    long bytesProcessed = 44; // Start after header
    
    // Convert
    while ((bytesRead = fread(buffer, sizeof(short), bufferSize * channels, wav)) > 0) {
        if (channels == 1) {
            bytesWritten = lame_encode_buffer(gfp, buffer, NULL, bytesRead, mp3Buffer, bufferSize * 2);
        } else {
            bytesWritten = lame_encode_buffer_interleaved(gfp, buffer, bytesRead / channels, mp3Buffer, bufferSize * 2);
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
