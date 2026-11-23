import java.util.Properties
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// Release key, kept outside the repository: storeFile, storePassword, keyAlias
// and keyPassword. CI writes the same file from GitHub secrets. Without it the
// APK is signed with the local debug key.
val signingFile = file("${System.getProperty("user.home")}/.dubsiren-signing/signing.properties")
val releaseSigning = Properties().apply {
    if (signingFile.isFile) signingFile.inputStream().use { load(it) }
}

android {
    namespace = "com.capobuf.dubsiren"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.capobuf.dubsiren"
        minSdk = 26 // AAudio
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }
    }

    // pcb_front.json is shared with the Flutter app.
    sourceSets.getByName("main").assets.srcDirs("../../app/assets")

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    signingConfigs {
        if (!releaseSigning.isEmpty) {
            create("release") {
                storeFile = file(releaseSigning.getProperty("storeFile"))
                storePassword = releaseSigning.getProperty("storePassword")
                keyAlias = releaseSigning.getProperty("keyAlias")
                keyPassword = releaseSigning.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // Single sideloadable APK.
            signingConfig = signingConfigs.findByName("release") ?: signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}
