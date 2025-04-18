package com.wavtomp3

import com.facebook.react.ReactPackage
import com.facebook.react.bridge.NativeModule
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.uimanager.ViewManager
import com.facebook.react.TurboReactPackage
import com.facebook.react.module.model.ReactModuleInfo
import com.facebook.react.module.model.ReactModuleInfoProvider

class WavToMp3Package : TurboReactPackage() {
    override fun getModule(name: String, reactContext: ReactApplicationContext): NativeModule? {
        return when (name) {
            WavToMp3Module.NAME -> WavToMp3Module(reactContext)
            else -> null
        }
    }

    override fun getReactModuleInfoProvider(): ReactModuleInfoProvider {
        return ReactModuleInfoProvider {
            mapOf(
                WavToMp3Module.NAME to ReactModuleInfo(
                    WavToMp3Module.NAME,
                    WavToMp3Module.NAME,
                    false, // canOverrideExistingModule
                    false, // needsEagerInit
                    true, // hasConstants
                    false, // isCxxModule
                    true // isTurboModule
                )
            )
        }
    }

    override fun createViewManagers(reactContext: ReactApplicationContext): List<ViewManager<*, *>> {
        return emptyList()
    }
}
