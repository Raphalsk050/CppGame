cmake_minimum_required(VERSION 3.24)

project(CppGame LANGUAGES C CXX)

# Marching cubes e CPU-bound: a poligonizacao roda inteira por frame. Num
# gerador single-config (Ninja/Makefiles), nao definir CMAKE_BUILD_TYPE
# significa compilar SEM nenhuma flag de otimizacao - aqui isso e a diferenca
# de uma ordem de grandeza no tempo de frame. Release por padrao, mas sem
# sobrescrever escolha explicita de quem configura.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 Debug Release RelWithDebInfo MinSizeRel)
endif()

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
    GIT_SHALLOW 1
    GIT_PROGRESS TRUE
)
FetchContent_Declare(
    raylib_cpp
    GIT_REPOSITORY https://github.com/RobLoach/raylib-cpp.git
    GIT_TAG v6.0.3
    GIT_SHALLOW 1
    GIT_PROGRESS TRUE
)

# raygui: biblioteca de UI imediata, header-only, do mesmo autor da raylib.
# SOURCE_SUBDIR aponta para um diretorio inexistente de proposito - assim o
# FetchContent baixa o repositorio mas NAO faz add_subdirectory, o que evitaria
# arrastar junto os exemplos e ferramentas dele. So o cabecalho interessa.
FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4.0
    GIT_SHALLOW 1
    GIT_PROGRESS TRUE
    SOURCE_SUBDIR nao-configurar
)

FetchContent_MakeAvailable(raylib raylib_cpp raygui)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/app/Application.cpp
    src/app/PlayerController.cpp
    src/field/MetaballField.cpp
    src/mc/Polygonizer.cpp
    src/mc/SampleGrid.cpp
    src/mc/Tables.cpp
    src/render/DynamicMesh.cpp
    src/render/Frustum.cpp
    src/render/SurfaceShader.cpp
    src/world/Chunk.cpp
    src/world/ChunkBuilder.cpp
    src/world/ChunkManager.cpp
    src/world/Noise.cpp
    src/world/TerrainEdits.cpp
    src/world/TerrainGenerator.cpp
    src/world/TerrainSettingsSchema.cpp
    src/ui/SettingsPanel.cpp
    src/ui/raygui_impl.c
)

# Includes sao sempre relativos a src/ ("mc/Tables.hpp"), nunca ao diretorio
# do arquivo que inclui - assim o caminho do include mostra a camada.
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${raygui_SOURCE_DIR}/src
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_26)

target_link_libraries(${PROJECT_NAME} PRIVATE raylib_cpp raylib)
