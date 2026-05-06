#=================== Top-Level ===================
file(GLOB Source_Files__VR RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp" "*.h")

source_group("" FILES ${Source_Files__VR})
target_sources(libultraship PRIVATE ${Source_Files__VR})
