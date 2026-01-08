import org.jetbrains.compose.desktop.application.dsl.TargetFormat

plugins {
    kotlin("jvm") version "2.3.0"
    id("org.jetbrains.compose") version "1.9.3"
    id("org.jetbrains.kotlin.plugin.compose") version "2.3.0"
}

group = "com.suvmusic.micmute"
version = "1.0.0"

repositories {
    google()
    mavenCentral()
}

dependencies {
    // Compose Desktop
    implementation(compose.desktop.currentOs)
    implementation(compose.desktop.common)
    implementation(compose.material3)
    implementation(compose.materialIconsExtended)
    implementation(compose.components.resources)
    
    // Coroutines
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.8.1")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-swing:1.8.1")
}

compose.desktop {
    application {
        mainClass = "com.suvmusic.micmute.MainKt"
        
        nativeDistributions {
            targetFormats(TargetFormat.Msi, TargetFormat.Exe)
            packageName = "MicMute"
            packageVersion = "1.0.0"
            description = "Microphone Mute/Unmute Utility"
            vendor = "Suvojeet Sengupta"
            
            windows {
                menuGroup = "MicMute"
                upgradeUuid = "8d3b2c1a-4e5f-6a7b-8c9d-0e1f2a3b4c5d"
            }
        }
        
        buildTypes.release {
            proguard {
                isEnabled.set(false)
            }
        }
    }
}

kotlin {
    jvmToolchain(17)
}
