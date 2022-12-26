add_subdirectory(sink)
add_subdirectory(source)

target_sources(app
    PRIVATE
        collector.c
)
