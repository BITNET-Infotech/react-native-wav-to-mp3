import { EmitterSubscription,NativeEventEmitter, NativeModules, Platform } from 'react-native';

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
export enum WavToMp3Events {
  /**
   * Progress update event
   */
  Progress = 'onProgress'
}

/**
 * Interface for the native module implementation
 */
interface WavToMp3NativeModule {
  convertWavToMp3(inputPath: string, outputPath: string, options?: WavToMp3Options): Promise<string>;
  convertAacToMp3?(inputPath: string, outputPath: string, options?: WavToMp3Options): Promise<string>;
}

const LINKING_ERROR =
  `The package '@bitnet-infotech/react-native-wav-to-mp3' doesn't seem to be linked. Make sure: \n\n${
  Platform.select({ ios: "- You have run 'pod install'\n", default: '' })
  }- You rebuilt the app after installing the package\n` +
  `- You are not using Expo Go\n`;

// Get the native module
const NativeWavToMp3 = NativeModules.WavToMp3
  ? NativeModules.WavToMp3
  : new Proxy(
      {},
      {
        get() {
          throw new Error(LINKING_ERROR);
        }
      }
    );

/**
 * Event emitter for conversion progress updates
 */
class WavToMp3Emitter {
  private eventEmitter: NativeEventEmitter;

  constructor() {
    this.eventEmitter = new NativeEventEmitter(NativeWavToMp3);
  }

  /**
   * Add a listener for conversion progress events
   * @param callback Function to be called with progress updates
   * @returns Subscription that should be removed when no longer needed
   */
  addProgressListener(callback: (progress: ConversionProgress) => void): EmitterSubscription {
    return this.eventEmitter.addListener(WavToMp3Events.Progress, callback);
  }

  /**
   * Remove all event listeners
   */
  removeAllListeners(): void {
    this.eventEmitter.removeAllListeners(WavToMp3Events.Progress);
  }
}

/**
 * Main WAV to MP3 converter class
 */
class WavToMp3Converter {
  private nativeModule: WavToMp3NativeModule;

  public events: WavToMp3Emitter;

  constructor() {
    this.nativeModule = NativeWavToMp3;
    this.events = new WavToMp3Emitter();
  }

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
  async convert(
    inputPath: string,
    outputPath: string,
    options?: WavToMp3Options
  ): Promise<string> {
    // Validate options
    if (options) {
      let processedOptions: WavToMp3Options = {};

      // Handle bitrate
      if (options.bitrate !== undefined) {
        const bitrate = Number(options.bitrate);
        if (isNaN(bitrate)) {
          throw new Error('Bitrate must be a valid number');
        }
        if (bitrate < 32 || bitrate > 320) {
          throw new Error('Bitrate must be between 32 and 320 kbps');
        }
        processedOptions.bitrate = bitrate;
      }

      // Handle quality
      if (options.quality !== undefined) {
        const quality = Number(options.quality);
        if (isNaN(quality)) {
          throw new Error('Quality must be a valid number');
        }
        if (quality < 0 || quality > 9) {
          throw new Error('Quality must be between 0 (best) and 9 (worst)');
        }
        processedOptions.quality = quality;
      }

      return this.nativeModule.convertWavToMp3(inputPath, outputPath, processedOptions);
    }

    return this.nativeModule.convertWavToMp3(inputPath, outputPath, options);
  }

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
  async convertAac(
    inputPath: string,
    outputPath: string,
    options?: WavToMp3Options
  ): Promise<string> {
    // Check if AAC conversion is supported (Android only)
    if (Platform.OS !== 'android') {
      throw new Error('AAC to MP3 conversion is only supported on Android');
    }

    if (!this.nativeModule.convertAacToMp3) {
      throw new Error('AAC to MP3 conversion is not available in this version');
    }

    // Validate options
    if (options) {
      let processedOptions: WavToMp3Options = {};

      // Handle bitrate
      if (options.bitrate !== undefined) {
        const bitrate = Number(options.bitrate);
        if (isNaN(bitrate)) {
          throw new Error('Bitrate must be a valid number');
        }
        if (bitrate < 32 || bitrate > 320) {
          throw new Error('Bitrate must be between 32 and 320 kbps');
        }
        processedOptions.bitrate = bitrate;
      }

      // Handle quality
      if (options.quality !== undefined) {
        const quality = Number(options.quality);
        if (isNaN(quality)) {
          throw new Error('Quality must be a valid number');
        }
        if (quality < 0 || quality > 9) {
          throw new Error('Quality must be between 0 (best) and 9 (worst)');
        }
        processedOptions.quality = quality;
      }

      return this.nativeModule.convertAacToMp3(inputPath, outputPath, processedOptions);
    }

    return this.nativeModule.convertAacToMp3(inputPath, outputPath, options);
  }
}

// Export a singleton instance
export const wavToMp3 = new WavToMp3Converter();

// Also export the class for those who want to create their own instances
export { WavToMp3Converter };

// Default export the singleton instance
export default wavToMp3; 