cmake_minimum_required(VERSION 3.21)

project(RetroMail
    VERSION 0.1.0
    DESCRIPTION "A 1998-style multi-account email client"
    HOMEPAGE_URL "https://github.com/jalovisko/retromail"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
# OAuth client IDs. Gmail and Outlook need these for "sign in with browser".
# They can also be supplied at runtime through environment variables of the
# same name. See README.md for how to register them.
set(RETROMAIL_GOOGLE_CLIENT_ID     "" CACHE STRING "Google OAuth client ID (Desktop app)")
set(RETROMAIL_GOOGLE_CLIENT_SECRET "" CACHE STRING "Google OAuth client secret (Desktop app)")
set(RETROMAIL_MICROSOFT_CLIENT_ID  "" CACHE STRING "Microsoft Entra application (client) ID")

# Build QtKeychain from source and link it statically. Handy for macOS and
# Windows packages, where no system copy exists.
option(RETROMAIL_FETCH_QTKEYCHAIN "Always build QtKeychain from source" OFF)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Network NetworkAuth)

if(NOT RETROMAIL_FETCH_QTKEYCHAIN)
    find_package(Qt6Keychain QUIET)
endif()

if(Qt6Keychain_FOUND)
    message(STATUS "Using system QtKeychain")
    set(RETROMAIL_KEYCHAIN_TARGET Qt6Keychain::Qt6Keychain)
else()
    message(STATUS "Building QtKeychain from source")
    include(FetchContent)
    set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
    set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)
    set(QTKEYCHAIN_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_Declare(qtkeychain
        GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
        GIT_TAG 0.15.0
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(qtkeychain)
    set(RETROMAIL_KEYCHAIN_TARGET qt6keychain)
endif()

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
configure_file(src/BuildConfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/BuildConfig.h @ONLY)

# Mail engine (no UI): accounts, IMAP, SMTP, OAuth, MIME. Also used by tests.
add_library(retromail_core STATIC
    src/core/Account.h            src/core/Account.cpp
    src/core/AccountStore.h       src/core/AccountStore.cpp
    src/core/SecretStore.h        src/core/SecretStore.cpp
    src/core/MimeMessage.h        src/core/MimeMessage.cpp
    src/core/MailSession.h        src/core/MailSession.cpp
    src/core/ImapSession.h        src/core/ImapSession.cpp
    src/core/DemoSession.h        src/core/DemoSession.cpp
    src/net/ImapClient.h          src/net/ImapClient.cpp
    src/net/SmtpClient.h          src/net/SmtpClient.cpp
    src/net/OAuthLogin.h          src/net/OAuthLogin.cpp
)
target_include_directories(retromail_core PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(retromail_core PUBLIC
    Qt6::Gui Qt6::Network Qt6::NetworkAuth ${RETROMAIL_KEYCHAIN_TARGET})

set(RETROMAIL_SOURCES
    src/main.cpp
    src/ui/Theme.h                src/ui/Theme.cpp
    src/ui/RetroWidgets.h         src/ui/RetroWidgets.cpp
    src/ui/MailboxArt.h           src/ui/MailboxArt.cpp
    src/ui/LoginScreen.h          src/ui/LoginScreen.cpp
    src/ui/MailboxScreen.h        src/ui/MailboxScreen.cpp
    src/ui/InboxScreen.h          src/ui/InboxScreen.cpp
    src/ui/ReaderScreen.h         src/ui/ReaderScreen.cpp
    src/ui/ComposeScreen.h        src/ui/ComposeScreen.cpp
    src/ui/AccountDialog.h        src/ui/AccountDialog.cpp
    src/ui/MainWindow.h           src/ui/MainWindow.cpp
)

if(APPLE)
    set(RETROMAIL_ICNS ${CMAKE_CURRENT_SOURCE_DIR}/assets/retromail.icns)
    set_source_files_properties(${RETROMAIL_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND RETROMAIL_SOURCES ${RETROMAIL_ICNS})
elseif(WIN32)
    list(APPEND RETROMAIL_SOURCES packaging/windows/retromail.rc)
endif()

qt_add_executable(retromail WIN32 MACOSX_BUNDLE ${RETROMAIL_SOURCES})
qt_add_resources(retromail "assets" PREFIX "/" FILES assets/retromail-256.png)
target_link_libraries(retromail PRIVATE retromail_core Qt6::Widgets)

if(APPLE OR WIN32)
    set_target_properties(retromail PROPERTIES OUTPUT_NAME RetroMail)
endif()

set_target_properties(retromail PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.jalovisko.retromail
    MACOSX_BUNDLE_BUNDLE_NAME RetroMail
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_ICON_FILE retromail.icns)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(RETROMAIL_BUILD_TESTS "Build unit tests" ON)
if(RETROMAIL_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    qt_add_executable(tst_mime tests/tst_mime.cpp)
    target_link_libraries(tst_mime PRIVATE retromail_core Qt6::Test)
    add_test(NAME mime COMMAND tst_mime)
    set_tests_properties(mime PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS retromail
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/retromail.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/linux/retromail.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES assets/retromail.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    foreach(size 16 32 48 64 128 256)
        install(FILES assets/retromail-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME retromail.png)
    endforeach()
else()
    # Copies the Qt libraries and plugins next to the app
    # (windeployqt on Windows, macdeployqt on macOS).
    qt_generate_deploy_app_script(
        TARGET retromail
        OUTPUT_SCRIPT RETROMAIL_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${RETROMAIL_DEPLOY_SCRIPT})
endif()

# ---------------------------------------------------------------------------
# Packages (cpack)
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "retromail")
set(CPACK_PACKAGE_VENDOR "RetroMail")
set(CPACK_PACKAGE_CONTACT "RetroMail <noreply@github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "RetroMail")
set(CPACK_PACKAGE_EXECUTABLES "RetroMail" "RetroMail")
set(CPACK_SOURCE_IGNORE_FILES "/\\\\.git/" "/build.*/" "/dist/")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "RetroMail")
    set(CPACK_PACKAGE_FILE_NAME "RetroMail-${PROJECT_VERSION}-macOS")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_PACKAGE_FILE_NAME "RetroMail-${PROJECT_VERSION}-Windows")
    set(CPACK_NSIS_DISPLAY_NAME "RetroMail")
    set(CPACK_NSIS_PACKAGE_NAME "RetroMail")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/retromail.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/retromail.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\RetroMail.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_CREATE_DESKTOP_LINKS "RetroMail")
    set(CPACK_NSIS_URL_INFO_ABOUT "${PROJECT_HOMEPAGE_URL}")
else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_SECTION "mail")
    # On Debian/Ubuntu, dpkg-shlibdeps works out the exact library packages.
    # Elsewhere (e.g. building a .deb on Arch), fall back to a fixed list.
    find_program(DPKG_SHLIBDEPS dpkg-shlibdeps)
    if(DPKG_SHLIBDEPS)
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    else()
        set(CPACK_DEBIAN_PACKAGE_DEPENDS
            "libqt6widgets6 (>= 6.9), libqt6network6 (>= 6.9), libqt6networkauth6 (>= 6.9), libqt6keychain1")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
        endif()
    endif()
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
endif()

include(CPack)
