plugins {
    id("com.android.application") version "8.13.2" apply false
    id("org.jetbrains.kotlin.android") version "2.2.21" apply false
}

// The repository sits in OneDrive, which keeps files open while syncing and
// makes packageRelease fail deleting its temp folder: build outside the tree.
allprojects {
    layout.buildDirectory.set(file("${System.getProperty("user.home")}/.dubsiren-android/${project.name}"))
}
