file(
    GLOB_RECURSE
    MY_SRC
    "*.cpp"
    "*.h"
)

add_executable(test-tb ${MY_SRC})

target_link_libraries(test-tb autopas autopasTools)
