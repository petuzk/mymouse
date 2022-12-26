add_subdirectory(sink)

target_sources(app
    PRIVATE
        collector.c
)
