cmake_minimum_required(VERSION 3.16)

project(linux-paint
    VERSION 1.0.0
    DESCRIPTION "MS Paint clone for Linux"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets PrintSupport)

# libmypaint — движок кистей MyPaint, он же в Krita и GIMP. Необязателен:
# без него кисти рисует собственный движок, и сборка идёт как прежде.
# Ставится пакетом libmypaint-dev.
option(WITH_LIBMYPAINT "Использовать libmypaint для кистей, если она есть" ON)

set(HAVE_LIBMYPAINT OFF)
if(WITH_LIBMYPAINT)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBMYPAINT QUIET libmypaint)
        if(LIBMYPAINT_FOUND)
            set(HAVE_LIBMYPAINT ON)
            message(STATUS "libmypaint ${LIBMYPAINT_VERSION} найдена — кисти ведёт она")
        endif()
    endif()
endif()
if(NOT HAVE_LIBMYPAINT)
    message(STATUS "libmypaint не найдена — кисти рисует собственный движок "
                   "(поставить: sudo apt install libmypaint-dev)")
endif()

set(PAINT_SOURCES
    src/main.cpp
    src/Document.cpp
    src/Canvas.cpp
    src/MainWindow.cpp
    src/Ribbon.cpp
    src/ColorArea.cpp
    src/LayersPanel.cpp
    src/Backdrop.cpp
    src/Icons.cpp
    src/Theme.cpp
    src/Dialogs.cpp
    src/tools/Tool.cpp
    src/tools/MyPaintEngine.cpp
    src/tools/PaintTools.cpp
    src/tools/ShapeTool.cpp
    src/tools/SelectTool.cpp
    src/tools/TextTool.cpp
)

set(PAINT_HEADERS
    src/Document.h
    src/Canvas.h
    src/MainWindow.h
    src/Ribbon.h
    src/ColorArea.h
    src/LayersPanel.h
    src/Backdrop.h
    src/WallpaperData.h
    src/FluentIcons.h
    src/Icons.h
    src/LogoData.h
    src/Theme.h
    src/Dialogs.h
    src/tools/Tool.h
    src/tools/MyPaintEngine.h
    src/tools/PaintTools.h
    src/tools/ShapeTool.h
    src/tools/SelectTool.h
    src/tools/TextTool.h
)

add_executable(linux-paint ${PAINT_SOURCES} ${PAINT_HEADERS})

target_include_directories(linux-paint PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(linux-paint PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport)

if(HAVE_LIBMYPAINT)
    target_compile_definitions(linux-paint PRIVATE HAVE_LIBMYPAINT)
    target_include_directories(linux-paint PRIVATE ${LIBMYPAINT_INCLUDE_DIRS})
    target_link_libraries(linux-paint PRIVATE ${LIBMYPAINT_LIBRARIES})
    target_link_directories(linux-paint PRIVATE ${LIBMYPAINT_LIBRARY_DIRS})
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(linux-paint PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

include(GNUInstallDirs)

install(TARGETS linux-paint
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Файлы проекта редактируются на Windows и приезжают с переносами CRLF.
# Для .desktop и для сценариев пакета это фатально: лишний символ попадает
# в значение Exec= и в строку #!/bin/sh. Поэтому оба прогоняются через
# configure_file с принудительным переводом строк в unix-вид.
configure_file(resources/linux-paint.desktop
    ${CMAKE_CURRENT_BINARY_DIR}/linux-paint.desktop
    @ONLY NEWLINE_STYLE LF)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/linux-paint.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

# Тот же логотип, что вшит в бинарник, ставится и как значок приложения
# в системе — чтобы в меню и на панели задач была одна и та же картинка.
install(FILES logo.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
    RENAME linux-paint.svg)

# --- сборка deb-пакета ------------------------------------------------------
# Пакет собирается из тех же правил install() выше, отдельного описания
# содержимого не требуется.

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "linux-paint")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "InoMotion")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "LinuxPaint - raster image editor")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://inomotion.pages.dev")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Konstantin Gorbunov <inomotion.pages.dev>")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
# Только подробный текст: строку-заголовок Debian берёт из
# CPACK_PACKAGE_DESCRIPTION_SUMMARY и подставляет сам. Если повторить её
# здесь, в окне установщика описание задвоится.
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
"A Paint-style raster editor for Linux: brushes with distinct textures,
 shapes with editable handles, selections, text, layers and the usual
 image operations. Built with Qt 6.")

# Имя файла в принятом Debian виде: linux-paint_1.0.0_amd64.deb
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Зависимости от библиотек Qt вычисляются автоматически по самому бинарнику.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# Эти модули загружаются во время работы и в бинарнике не упомянуты,
# поэтому автоматический разбор их не находит — указываем руками.
# Без qt6-qpa-plugins окно вообще не откроется, так что это жёсткая
# зависимость.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins, hicolor-icon-theme")
if(HAVE_LIBMYPAINT)
    # Собрались с движком кистей — значит, он нужен и на машине пользователя.
    string(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS ", libmypaint-1.5-1 | libmypaint0")
endif()

# А вот модуль отрисовки SVG нужен только для логотипа: без него программа
# работает и рисует запасной значок. В разных дистрибутивах он называется
# по-разному, и жёсткая зависимость на несуществующее имя сорвала бы всю
# установку — поэтому он в рекомендуемых.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-svg-plugins")

# Сценарии обновляют кэши меню, значков и типов файлов после установки.
# Берём не исходные файлы, а их копии с unix-переносами строк.
configure_file(packaging/postinst ${CMAKE_CURRENT_BINARY_DIR}/packaging/postinst
    @ONLY NEWLINE_STYLE LF)
configure_file(packaging/postrm ${CMAKE_CURRENT_BINARY_DIR}/packaging/postrm
    @ONLY NEWLINE_STYLE LF)

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_BINARY_DIR}/packaging/postinst"
    "${CMAKE_CURRENT_BINARY_DIR}/packaging/postrm")
# Сценариям нужен бит выполнения — иначе dpkg их не запустит.
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

include(CPack)

# --- сборка AppImage --------------------------------------------------------
# Отдельная цель: cmake --build <каталог сборки> --target appimage
# Собирает один файл, который несёт Qt внутри себя и потому не зависит от
# версии Qt в системе пользователя.

add_custom_target(appimage
    COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/packaging/appimage.cmake
    DEPENDS linux-paint
    COMMENT "Сборка AppImage"
    VERBATIM)
