# Сборка AppImage — одного файла, который запускается на любом дистрибутиве.
#
# Запускается не напрямую, а целью сборки:
#     cmake --build <каталог сборки> --target appimage
#
# Идея AppImage в том, что нужные библиотеки Qt укладываются внутрь файла.
# Поэтому системная версия Qt на машине пользователя перестаёт иметь
# значение — именно этого нельзя добиться обычным deb-пакетом.
#
# Скрипт написан на языке CMake намеренно: он не зависит от оболочки,
# прав на выполнение и вида переносов строк, из-за которых файлы,
# отредактированные на Windows, обычно не запускаются.

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "Не задан BUILD_DIR. Запускайте через цель appimage.")
endif()

set(APPDIR "${BUILD_DIR}/AppDir")
set(TOOLS "${BUILD_DIR}/appimage-tools")

# --- 1. Раскладываем программу по обычным системным путям внутри AppDir ---

message(STATUS "Готовим AppDir")
file(REMOVE_RECURSE "${APPDIR}")
file(MAKE_DIRECTORY "${TOOLS}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "DESTDIR=${APPDIR}"
            "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix /usr
    RESULT_VARIABLE install_result)

if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Не удалось разложить файлы в AppDir")
endif()

# --- 2. Забираем инструменты сборки, если их ещё нет ---

function(fetch_tool url path)
    if(EXISTS "${path}")
        return()
    endif()

    message(STATUS "Загрузка ${url}")
    file(DOWNLOAD "${url}" "${path}" SHOW_PROGRESS STATUS status)

    list(GET status 0 code)
    if(NOT code EQUAL 0)
        list(GET status 1 message_text)
        # Оборванную закачку обязательно удаляем: иначе при следующем
        # запуске обрывок примут за готовый инструмент.
        file(REMOVE "${path}")
        message(FATAL_ERROR "Не удалось скачать ${url}: ${message_text}")
    endif()

    execute_process(COMMAND chmod +x "${path}")
endfunction()

set(LINUXDEPLOY "${TOOLS}/linuxdeploy-x86_64.AppImage")
set(LINUXDEPLOY_QT "${TOOLS}/linuxdeploy-plugin-qt-x86_64.AppImage")

fetch_tool(
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    "${LINUXDEPLOY}")
fetch_tool(
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    "${LINUXDEPLOY_QT}")

# --- 3. Собираем ---

# Модулю Qt нужен qmake, чтобы понять, где лежат библиотеки и модули.
find_program(QMAKE_EXECUTABLE NAMES qmake6 qmake-qt6 qmake)
if(NOT QMAKE_EXECUTABLE)
    message(FATAL_ERROR
        "Не найден qmake6. Установите qt6-base-dev-tools и повторите.")
endif()

message(STATUS "Сборка AppImage, это займёт минуту")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            # Позволяет работать без FUSE — его часто нет в контейнерах
            # и на свежих системах.
            "APPIMAGE_EXTRACT_AND_RUN=1"
            "QMAKE=${QMAKE_EXECUTABLE}"
            "OUTPUT=LinuxPaint-x86_64.AppImage"
            # Модуль отрисовки SVG нужен для логотипа. Он загружается во
            # время работы и в бинарнике не упомянут, поэтому сам по себе
            # в сборку не попадает — просим уложить его явно.
            "EXTRA_QT_PLUGINS=svg"
            # linuxdeploy ищет свои модули по PATH и рядом с собой.
            "PATH=${TOOLS}:$ENV{PATH}"
            "${LINUXDEPLOY}"
            --appdir "${APPDIR}"
            --plugin qt
            --output appimage
    WORKING_DIRECTORY "${BUILD_DIR}"
    RESULT_VARIABLE build_result)

if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "linuxdeploy завершился с ошибкой")
endif()

message(STATUS "Готово: ${BUILD_DIR}/LinuxPaint-x86_64.AppImage")
