if(NOT DEFINED WINDEPLOYQT_EXECUTABLE
   OR NOT DEFINED QML_SOURCE_DIRECTORY
   OR NOT DEFINED APPLICATION_FILE
   OR NOT DEFINED APPLICATION_DIRECTORY)
    message(FATAL_ERROR "DeployQtRuntime.cmake requires all deployment paths")
endif()

execute_process(
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
        --release
        --dir "${APPLICATION_DIRECTORY}"
        --qmldir "${QML_SOURCE_DIRECTORY}"
        --no-translations
        --no-opengl-sw
        --no-system-d3d-compiler
        --no-system-dxc-compiler
        --verbose 0
        --skip-plugin-types generic,qmltooling,networkinformation
        --exclude-plugins qgif,qico,qjpeg
        --no-quickcontrols2fusion
        --no-quickcontrols2fusionstyleimpl
        --no-quickcontrols2imagine
        --no-quickcontrols2imaginestyleimpl
        --no-quickcontrols2material
        --no-quickcontrols2materialstyleimpl
        --no-quickcontrols2universal
        --no-quickcontrols2universalstyleimpl
        --no-quickcontrols2fluentwinui3styleimpl
        --no-quickcontrols2windowsstyleimpl
        --no-quickeffects
        "${APPLICATION_FILE}"
    RESULT_VARIABLE deploy_result
)

if(NOT deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt failed with exit code ${deploy_result}")
endif()

# windeployqt does not remove files left by an older, broader deployment and
# qmlimportscanner deploys every available Controls style. DME uses Basic.
set(obsolete_runtime_paths
    "D3Dcompiler_47.dll"
    "DME_ui_preview.exe"
    "opengl32sw.dll"
    "Qt6Quick3DUtils.dll"
    "Qt6QuickControls2FluentWinUI3StyleImpl.dll"
    "Qt6QuickControls2Fusion.dll"
    "Qt6QuickControls2FusionStyleImpl.dll"
    "Qt6QuickControls2Imagine.dll"
    "Qt6QuickControls2ImagineStyleImpl.dll"
    "Qt6QuickControls2Material.dll"
    "Qt6QuickControls2MaterialStyleImpl.dll"
    "Qt6QuickControls2Universal.dll"
    "Qt6QuickControls2UniversalStyleImpl.dll"
    "Qt6QuickControls2WindowsStyleImpl.dll"
    "Qt6QuickEffects.dll"
    "Qt6QuickShapes.dll"
    "generic"
    "networkinformation"
    "qmltooling"
    "imageformats/qgif.dll"
    "imageformats/qico.dll"
    "imageformats/qjpeg.dll"
    "tls/qcertonlybackend.dll"
    "tls/qopensslbackend.dll"
    "qml/QtQuick/Controls/FluentWinUI3"
    "qml/QtQuick/Controls/Fusion"
    "qml/QtQuick/Controls/Imagine"
    "qml/QtQuick/Controls/Material"
    "qml/QtQuick/Controls/Universal"
    "qml/QtQuick/Controls/Windows"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Fusion"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Imagine"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Material"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Universal"
    "qml/QtQuick/Effects"
    "qml/QtQuick/LocalStorage"
    "qml/QtQuick/NativeStyle"
    "qml/QtQuick/Particles"
    "qml/QtQuick/Shapes"
    "qml/QtQuick/Timeline"
    "qml/QtQuick/VectorImage"
    "qml/QtQuick/tooling"
    "qml/QtQml/XmlListModel"
)

foreach(relative_path IN LISTS obsolete_runtime_paths)
    set(absolute_path "${APPLICATION_DIRECTORY}/${relative_path}")
    if(IS_DIRECTORY "${absolute_path}")
        file(REMOVE_RECURSE "${absolute_path}")
    elseif(EXISTS "${absolute_path}")
        file(REMOVE "${absolute_path}")
    endif()
endforeach()
