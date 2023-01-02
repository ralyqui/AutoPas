file(
    GLOB_RECURSE
    MY_SRC
    "*.cpp"
    "*.h"
)

add_executable(test-at ${MY_SRC})

target_link_libraries(test-at autopas autopasTools)
