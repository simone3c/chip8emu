add_library(${PROJECT_NAME}_lib)
target_sources("${PROJECT_NAME}_lib"
    PRIVATE chip8.cpp
)