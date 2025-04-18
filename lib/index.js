"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.WavToMp3Converter = exports.wavToMp3 = exports.WavToMp3Events = void 0;
const react_native_1 = require("react-native");
/**
 * Event types that can be emitted by the converter
 */
var WavToMp3Events;
(function (WavToMp3Events) {
    /**
     * Progress update event
     */
    WavToMp3Events["Progress"] = "onProgress";
})(WavToMp3Events = exports.WavToMp3Events || (exports.WavToMp3Events = {}));
const LINKING_ERROR = `The package '@bitnet-infotech/react-native-wav-to-mp3' doesn't seem to be linked. Make sure: \n\n${react_native_1.Platform.select({ ios: "- You have run 'pod install'\n", default: '' })}- You rebuilt the app after installing the package\n` +
    `- You are not using Expo Go\n`;
// Get the native module
const NativeWavToMp3 = react_native_1.NativeModules.WavToMp3
    ? react_native_1.NativeModules.WavToMp3
    : new Proxy({}, {
        get() {
            throw new Error(LINKING_ERROR);
        }
    });
/**
 * Event emitter for conversion progress updates
 */
class WavToMp3Emitter {
    constructor() {
        this.eventEmitter = new react_native_1.NativeEventEmitter(NativeWavToMp3);
    }
    /**
     * Add a listener for conversion progress events
     * @param callback Function to be called with progress updates
     * @returns Subscription that should be removed when no longer needed
     */
    addProgressListener(callback) {
        return this.eventEmitter.addListener(WavToMp3Events.Progress, callback);
    }
    /**
     * Remove all event listeners
     */
    removeAllListeners() {
        this.eventEmitter.removeAllListeners(WavToMp3Events.Progress);
    }
}
/**
 * Main WAV to MP3 converter class
 */
class WavToMp3Converter {
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
    convert(inputPath, outputPath, options) {
        return __awaiter(this, void 0, void 0, function* () {
            return this.nativeModule.convertWavToMp3(inputPath, outputPath, options);
        });
    }
}
exports.WavToMp3Converter = WavToMp3Converter;
// Export a singleton instance
exports.wavToMp3 = new WavToMp3Converter();
// Default export the singleton instance
exports.default = exports.wavToMp3;
