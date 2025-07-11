package com.wavtomp3

import android.util.Log
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.ReactMethod
import com.facebook.react.bridge.Promise
import com.facebook.react.bridge.ReadableMap
import com.facebook.react.module.annotations.ReactModule
import java.io.File

@ReactModule(name = WavToMp3Module.NAME)
class WavToMp3Module(reactContext: ReactApplicationContext) :
  ReactContextBaseJavaModule(reactContext) {

  init {
    System.loadLibrary("wav-to-mp3")
  }

  override fun getName(): String {
    return NAME
  }

  @ReactMethod
  fun convertWavToMp3(inputPath: String, outputPath: String, options: ReadableMap?, promise: Promise) {
    convertAudioToMp3(inputPath, outputPath, "wav", options, promise)
  }

  @ReactMethod
  fun convertAacToMp3(inputPath: String, outputPath: String, options: ReadableMap?, promise: Promise) {
    convertAudioToMp3(inputPath, outputPath, "aac", options, promise)
  }

  @ReactMethod
  fun convertAudioToMp3(inputPath: String, outputPath: String, inputFormat: String, options: ReadableMap?, promise: Promise) {
    try {
      // Remove file:// prefix if present and clean up path
      var processedInputPath = inputPath
      var processedOutputPath = outputPath
      
      if (inputPath.startsWith("file://")) {
        processedInputPath = inputPath.substring(7)
        // Remove any leading double slashes
        if (processedInputPath.startsWith("//")) {
          processedInputPath = processedInputPath.substring(1)
        }
      }
      if (outputPath.startsWith("file://")) {
        processedOutputPath = outputPath.substring(7)
        // Remove any leading double slashes
        if (processedOutputPath.startsWith("//")) {
          processedOutputPath = processedOutputPath.substring(1)
        }
      }
      
      // Ensure output directory exists
      val outputFile = File(processedOutputPath)
      val outputDir = outputFile.parentFile
      if (outputDir != null && !outputDir.exists()) {
        val created = outputDir.mkdirs()
        if (!created) {
          promise.reject("DIRECTORY_ERROR", "Failed to create output directory: ${outputDir.absolutePath}")
          return
        }
      }
      
      // Log file paths and sizes
      val inputFile = File(processedInputPath)
      Log.d(TAG, "Input path: $processedInputPath")
      Log.d(TAG, "Input format: $inputFormat")
      Log.d(TAG, "Input file exists: ${inputFile.exists()}")
      if (inputFile.exists()) {
        Log.d(TAG, "Input file size: ${inputFile.length()} bytes")
      }
      
      Log.d(TAG, "Output path: $processedOutputPath")
      
      // Get options with defaults
      val bitrate = options?.getInt("bitrate") ?: -1
      val quality = options?.getInt("quality") ?: -1
      
      val result = nativeConvertAudioToMp3(processedInputPath, processedOutputPath, inputFormat, bitrate, quality)
      
      // Log output file size after conversion
      val resultFile = File(processedOutputPath)
      Log.d(TAG, "Output file exists: ${resultFile.exists()}")
      if (resultFile.exists()) {
        Log.d(TAG, "Output file size: ${resultFile.length()} bytes")
      }
      
      if (result == 0) {
        promise.resolve(processedOutputPath)
      } else {
        promise.reject("CONVERSION_ERROR", "Failed to convert audio file from $inputFormat to MP3")
      }
    } catch (e: Exception) {
      promise.reject("CONVERSION_ERROR", e.message)
    }
  }

  private external fun nativeConvertWavToMp3(inputPath: String, outputPath: String, bitrate: Int?, quality: Int?): Int
  private external fun nativeConvertAudioToMp3(inputPath: String, outputPath: String, inputFormat: String, bitrate: Int?, quality: Int?): Int

  companion object {
    const val NAME = "WavToMp3"
    private const val TAG = "WavToMp3"
  }
}
