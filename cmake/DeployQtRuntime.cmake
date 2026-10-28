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
        --no-system-dxc-compiler
        --verbose 0
        --skip-plugin-types generic,qmltooling,networkinformation
        --exclude-plugins qgif,qico,qjpeg,qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql,qsqlmysql
        --include-plugins qsqlite
        --no-quickcontrols2imagine
        --no-quickcontrols2imaginestyleimpl
        --no-quickcontrols2material
        --no-quickcontrols2materialstyleimpl
        --no-quickcontrols2universal
        --no-quickcontrols2universalstyleimpl
        --no-quickcontrols2windowsstyleimpl
        "${APPLICATION_FILE}"
    RESULT_VARIABLE deploy_result
)

# Resolve the deployed QML modules and SQLite driver relative to the executable,
# including Qt builds whose compiled prefix still points at a development kit.
file(WRITE "${APPLICATION_DIRECTORY}/qt.conf"
    "[Paths]\nPrefix=.\nPlugins=.\nQmlImports=qml\nTranslations=translations\n")

if(NOT deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt failed with exit code ${deploy_result}")
endif()

# Activity uses a local SQLite queue; other SQL drivers require client libraries
# that do not belong in DME. Also remove drivers left by earlier deployments.
file(GLOB deployed_sql_drivers "${APPLICATION_DIRECTORY}/sqldrivers/*.dll")
foreach(sql_driver IN LISTS deployed_sql_drivers)
    get_filename_component(sql_driver_name "${sql_driver}" NAME)
    if(NOT sql_driver_name STREQUAL "qsqlite.dll")
        file(REMOVE "${sql_driver}")
    endif()
endforeach()
if(NOT EXISTS "${APPLICATION_DIRECTORY}/sqldrivers/qsqlite.dll")
    message(FATAL_ERROR "The SQLite driver required by DME Activity was not deployed")
endif()

# windeployqt does not remove files left by an older, broader deployment and
# qmlimportscanner deploys every available Controls style. DME uses Basic.
set(obsolete_runtime_paths
    "DME_ui_preview.exe"
    "opengl32sw.dll"
    "Qt6Quick3DUtils.dll"
    "Qt6QuickControls2Imagine.dll"
    "Qt6QuickControls2ImagineStyleImpl.dll"
    "Qt6QuickControls2Material.dll"
    "Qt6QuickControls2MaterialStyleImpl.dll"
    "Qt6QuickControls2Universal.dll"
    "Qt6QuickControls2UniversalStyleImpl.dll"
    "Qt6QuickControls2WindowsStyleImpl.dll"
    "Qt6QuickParticles.dll"
    "Qt6QuickShapes.dll"
    "generic"
    "networkinformation"
    "qmltooling"
    "imageformats/qgif.dll"
    "imageformats/qico.dll"
    "imageformats/qjpeg.dll"
    "tls/qcertonlybackend.dll"
    "tls/qopensslbackend.dll"
    "qml/QtQuick/Controls/Imagine"
    "qml/QtQuick/Controls/Material"
    "qml/QtQuick/Controls/Universal"
    "qml/QtQuick/Controls/Windows"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Fusion"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Imagine"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Material"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Universal"
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
