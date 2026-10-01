# Local Android build environment

Installed for the DME Android ARM64 port on this Windows machine:

| Component | Version / location |
| --- | --- |
| Qt Android ARM64 | `C:/Qt/6.10.2/android_arm64_v8a` |
| Qt host tools | `C:/Qt/6.10.2/mingw_64` |
| Qt modules | Base, Declarative/Quick, SVG, Tools, Translations, Shader Tools; GuiPrivate headers included |
| JDK | Temurin 17.0.20.1, `C:/Qt/Tools/jdk-17/jdk-17.0.20.1+1` |
| Android SDK | `C:/Users/Dewral/AppData/Local/Android/Sdk` |
| SDK Platform | Android 36 |
| Build Tools | 36.0.0 |
| NDK | `27.2.12479018` under `Sdk/ndk` |
| Platform Tools / ADB | 37.0.1 under `Sdk/platform-tools` |
| Command-line tools | 22.0 under `Sdk/cmdline-tools/latest` |
| Google USB driver files | `Sdk/extras/google/usb_driver` |
| Android OpenSSL | `C:/Qt/Tools/android_openssl`, KDAB's Qt Android support repository |
| CMake / Ninja | Existing Windows installations, Ninja at `C:/Qt/Tools/Ninja/ninja.exe` |

Qt was downloaded using aqtinstall from the official Qt `all_os/android`
repository. It is not registered as an additional MaintenanceTool component.
JDK and Android command-line archives were checked against publisher SHA-256
checksums. SDK packages were installed using Google's SDK manager.

`JAVA_HOME`, `ANDROID_HOME`, `ANDROID_SDK_ROOT` and `ANDROID_NDK_ROOT` are set
in the Windows user environment. Restart terminals/Qt Creator to inherit
these values. The current-session helper is
`build/android-setup/android-env.ps1`; dot-source it before command-line builds.

In Qt Creator, select the JDK and SDK paths above in Android settings and
add `C:/Qt/6.10.2/android_arm64_v8a/bin/qmake.bat` as the Qt version if it is
not detected automatically. Use the matching Android ARM64 kit, not MinGW.

## Toolchain smoke project

A small Qt Quick application is located in `build/android-setup/smoke`.
It includes the QRhi development headers and packages the Android OpenSSL
libraries. Its generated output is separate from the DME application.

```powershell
. ./build/android-setup/android-env.ps1
& C:/Qt/6.10.2/android_arm64_v8a/bin/qt-cmake.bat `
  -S ./build/android-setup/smoke -B ./build/android-setup/smoke-build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DANDROID_PLATFORM=android-28 `
  -DQT_HOST_PATH=C:/Qt/6.10.2/mingw_64 `
  -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe
cmake --build ./build/android-setup/smoke-build --target apk --parallel
```

The Qt wrapper supplies Gradle 8.14.3. Gradle and Android plugin dependencies
are cached under the user's `.gradle` directory on the first APK build.

Validation completed successfully: the ARM64 C++/Qt Quick project compiled
and Gradle produced `android-build-release-unsigned.apk` (45,449,780 bytes)
under `build/android-setup/smoke-build/android-build/build/outputs/apk/release`.
This is an unsigned environment test APK, not a DME mobile release. It has
not been installed on a phone.

This setup targets a physical ARM64 Android device. An emulator/system image,
Android Studio and additional Qt ABIs are optional and were not installed.
No device was flashed or configured. DME still needs its Android packaging,
file-access and touch-interface changes before it becomes an Android app.

Sources: [Qt 6.10 Android prerequisites](https://doc.qt.io/qt-6.10/android-getting-started.html),
[Qt command-line Android builds](https://doc.qt.io/qt-6.10/android-building-projects-from-commandline.html),
[KDAB Android OpenSSL](https://github.com/KDAB/android_openssl).

## Emulator Androida (2026-09-14)

Zainstalowano system-images;android-36;google_apis;x86_64 i Qt 6.10.2 android_x86_64 z ShaderTools.
AVD: DME_Android_36, profil Pixel 7. Przyspieszenie WHPX sprawdzone i dostępne.
Test: emulator uruchomił system (sys.boot_completed=1), debug APK aplikacji AndroidToolchainSmoke został zbudowany, zainstalowany i uruchomiony (am start: Status ok).
Uruchamianie emulatora: build/android-setup/Start-Android-Emulator.cmd.
W Qt Creator dodaj Qt przez C:/Qt/6.10.2/android_x86_64/bin/qmake.bat i wybierz zestaw Android x86_64.
Pozostaje diagnoza odczytu pakietów w zwykłej sesji Qt Creator. SDK manager 19.0 działa w terminalu i w instancji diagnostycznej Qt Creator.
