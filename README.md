# @bitnet-infotech/react-native-wav-to-mp3

A React Native library for converting WAV audio files to MP3 format using the LAME encoder. This library provides native implementations for both iOS and Android platforms.

## Features

- Convert WAV files to MP3 format
- Progress tracking during conversion
- Configurable bitrate and quality settings
- Support for both iOS and Android platforms

## Installation

```bash
npm install @bitnet-infotech/react-native-wav-to-mp3
# or
yarn add @bitnet-infotech/react-native-wav-to-mp3
```

### iOS Setup

Run pod install in your iOS directory:
```bash
cd ios && pod install
```

### Android Setup

The Android implementation includes the LAME encoder, so no additional setup is required.

## Usage

```typescript
import wavToMp3 from '@bitnet-infotech/react-native-wav-to-mp3';

const convertAudio = async () => {
  try {
    const wavPath = 'file:///path/to/your/audio.wav';
    const mp3Path = 'file:///path/to/output/audio.mp3';
    
    // Add progress listener
    const subscription = wavToMp3.events.addProgressListener((progress) => {
      console.log(`Converting: ${(progress.progress * 100).toFixed(1)}%`);
    });

    const result = await wavToMp3.convert(wavPath, mp3Path, {
      bitrate: 192,  // 192kbps
      quality: 2     // High quality
    });
    
    console.log('Conversion successful:', result);
    
    // Clean up listener
    subscription.remove();
  } catch (error) {
    console.error('Conversion failed:', error);
  }
};
```

## API Reference

### Methods

#### `convert(inputPath: string, outputPath: string, options?: WavToMp3Options): Promise<string>`

Converts a WAV file to MP3 format using the LAME encoder.

##### Parameters

- `inputPath` (string): The file path to the source WAV file
  - Must be a valid file URL (starting with 'file:///')
  - Example: 'file:///path/to/audio.wav'

- `outputPath` (string): The file path where the converted MP3 should be saved
  - Must be a valid file URL (starting with 'file:///')
  - Example: 'file:///path/to/output.mp3'

- `options` (WavToMp3Options, optional): Conversion settings
  ```typescript
  interface WavToMp3Options {
    /**
     * MP3 encoding bitrate in kbps
     * @default 128
     */
    bitrate?: number;
    
    /**
     * Encoding quality (0=best, 9=worst)
     * @default 5
     */
    quality?: number;
  }
  ```

##### Returns

- `Promise<string>`: Resolves with the path to the converted MP3 file
- Rejects with an error if the conversion fails

### Events

#### Progress Tracking

```typescript
interface ConversionProgress {
  /**
   * Progress value between 0 and 1
   */
  progress: number;
}
```

To track conversion progress:

```typescript
const subscription = wavToMp3.events.addProgressListener((progress) => {
  console.log(`Progress: ${(progress.progress * 100).toFixed(1)}%`);
});

// Don't forget to remove the listener when done
subscription.remove();
```

### Error Handling

The conversion might fail for several reasons:
- Invalid input/output file paths
- Input file doesn't exist
- Input file is not a valid WAV file
- Insufficient permissions
- Device storage is full

## Example

```typescript
import wavToMp3 from '@bitnet-infotech/react-native-wav-to-mp3';
import RNFS from 'react-native-fs';

const convertWavToMp3Example = async () => {
  try {
    // Example paths - adjust according to your app's needs
    const wavPath = `${RNFS.DocumentDirectoryPath}/input.wav`;
    const mp3Path = `${RNFS.DocumentDirectoryPath}/output.mp3`;
    
    // Add progress listener
    const subscription = wavToMp3.events.addProgressListener((progress) => {
      console.log(`Converting: ${(progress.progress * 100).toFixed(1)}%`);
    });
    
    const result = await wavToMp3.convert(
      `file://${wavPath}`,
      `file://${mp3Path}`,
      {
        bitrate: 192,  // Higher quality audio
        quality: 2     // Better encoding quality
      }
    );
    
    console.log('MP3 file saved at:', result);
    
    // Clean up
    subscription.remove();
  } catch (error) {
    console.error('Conversion error:', error);
  }
};
```

## Requirements

- React Native >= 0.60.0
- iOS 11.0 or later
- Android API level 21 (Android 5.0) or later

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

MIT © [BITNET-Infotech](https://bitnetinfotech.com/)

## Support

For bugs, feature requests, or questions, please [file an issue](https://github.com/BITNET-Infotech/react-native-wav-to-mp3/issues). 