import { EmitterSubscription } from 'react-native';
/**
 * Options for WAV to MP3 conversion
 */
export interface WavToMp3Options {
    /**
     * MP3 encoding bitrate in kbps (default: 128)
     */
    bitrate?: number;
    /**
     * Encoding quality (0=best, 9=worst, default: 5)
     */
    quality?: number;
}
/**
 * Progress event data during conversion
 */
export interface ConversionProgress {
    /**
     * Progress value between 0 and 1
     */
    progress: number;
}
/**
 * Event types that can be emitted by the converter
 */
export declare enum WavToMp3Events {
    /**
     * Progress update event
     */
    Progress = "onProgress"
}
/**
 * Event emitter for conversion progress updates
 */
declare class WavToMp3Emitter {
    private eventEmitter;
    constructor();
    /**
     * Add a listener for conversion progress events
     * @param callback Function to be called with progress updates
     * @returns Subscription that should be removed when no longer needed
     */
    addProgressListener(callback: (progress: ConversionProgress) => void): EmitterSubscription;
    /**
     * Remove all event listeners
     */
    removeAllListeners(): void;
}
/**
 * Main WAV to MP3 converter class
 */
declare class WavToMp3Converter {
    private nativeModule;
    events: WavToMp3Emitter;
    constructor();
    /**
     * Convert a WAV file to MP3 format
     * @param inputPath Path to the input WAV file (can be file:// URI)
     * @param outputPath Path where the output MP3 file should be saved (can be file:// URI)
     * @param options Optional conversion settings
     * @returns Promise that resolves with the output file path when conversion is complete
     *
     * @example
     * ```typescript
     * const converter = new WavToMp3Converter();
     *
     * // Add progress listener
     * const subscription = converter.events.addProgressListener((progress) => {
     *   console.log(`Converting: ${(progress.progress * 100).toFixed(1)}%`);
     * });
     *
     * try {
     *   const outputPath = await converter.convert(
     *     'file:///input.wav',
     *     'file:///output.mp3',
     *     {
     *       bitrate: 192,  // 192kbps
     *       quality: 2     // High quality
     *     }
     *   );
     *   console.log('Conversion successful:', outputPath);
     * } catch (error) {
     *   console.error('Conversion failed:', error);
     * } finally {
     *   subscription.remove();
     * }
     * ```
     */
    convert(inputPath: string, outputPath: string, options?: WavToMp3Options): Promise<string>;
    /**
     * Convert an AAC file to MP3 format (Android only)
     * @param inputPath Path to the input AAC file (can be file:// URI)
     * @param outputPath Path where the output MP3 file should be saved (can be file:// URI)
     * @param options Optional conversion settings
     * @returns Promise that resolves with the output file path when conversion is complete
     *
     * @example
     * ```typescript
     * const converter = new WavToMp3Converter();
     *
     * // Add progress listener
     * const subscription = converter.events.addProgressListener((progress) => {
     *   console.log(`Converting: ${(progress.progress * 100).toFixed(1)}%`);
     * });
     *
     * try {
     *   const outputPath = await converter.convertAac(
     *     'file:///input.aac',
     *     'file:///output.mp3',
     *     {
     *       bitrate: 192,  // 192kbps
     *       quality: 2     // High quality
     *     }
     *   );
     *   console.log('Conversion successful:', outputPath);
     * } catch (error) {
     *   console.error('Conversion failed:', error);
     * } finally {
     *   subscription.remove();
     * }
     * ```
     */
    convertAac(inputPath: string, outputPath: string, options?: WavToMp3Options): Promise<string>;
}
export declare const wavToMp3: WavToMp3Converter;
export { WavToMp3Converter };
export default wavToMp3;
