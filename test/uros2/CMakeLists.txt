# uROS2 tests

# shared_block test
add_executable(shared_block_test
    shared_block/shared_block_test.cpp
)

target_include_directories(shared_block_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty
)

target_link_libraries(shared_block_test PRIVATE
    pthread
)

# Add test to CTest
add_test(NAME shared_block_test COMMAND shared_block_test)

# shared_block multi-threaded test
add_executable(shared_block_mt_test
    shared_block/shared_block_mt_test.cpp
)

target_include_directories(shared_block_mt_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty
)

target_link_libraries(shared_block_mt_test PRIVATE
    pthread
)

# Add test to CTest
add_test(NAME shared_block_mt_test COMMAND shared_block_mt_test)

# shared_block broadcast test (one producer, multiple consumers)
add_executable(shared_block_broadcast_test
    shared_block/shared_block_broadcast_test.cpp
)

target_include_directories(shared_block_broadcast_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty
)

target_link_libraries(shared_block_broadcast_test PRIVATE
    pthread
)

# Add test to CTest
add_test(NAME shared_block_broadcast_test COMMAND shared_block_broadcast_test)
