cmake_minimum_required(VERSION 3.16)

project(cutefish-photos VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(CutefishToolkit REQUIRED)
find_package(CutefishClients REQUIRED COMPONENTS appearance)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/photo.cpp
    src/glyphs.cpp
    src/grid.cpp
    src/detail.cpp
    src/sidebar.cpp
    src/infopanel.cpp
    src/photoswindow.cpp
)
target_compile_definitions(${PROJECT_NAME} PRIVATE PHOTOS_VERSION="${PROJECT_VERSION}")
target_include_directories(${PROJECT_NAME} PRIVATE src /usr/include/cutefish)
target_link_libraries(${PROJECT_NAME} PRIVATE Cutefish::Toolkit Cutefish::AppearanceClient)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES cutefish-photos.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/cutefish-photos.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
