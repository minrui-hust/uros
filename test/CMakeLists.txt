include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# tests for topic
add_executable(local_test local_test.cpp)

add_executable(remote_test_local remote_test_local.cpp)
add_executable(remote_test_remote remote_test_remote.cpp)

add_executable(remote_test_producer remote_test_producer.cpp)
add_executable(remote_test_relay remote_test_relay.cpp)
add_executable(remote_test_consumer remote_test_consumer.cpp)

# tests for service
add_executable(local_service_test local_service_test.cpp)

add_executable(remote_service_server remote_service_server.cpp)
add_executable(remote_service_client remote_service_client.cpp)

# multicast service tests
add_executable(multicast_service_server multicast_service_server.cpp)
add_executable(multicast_service_client multicast_service_client.cpp)

# bytestream tests
add_executable(bytestream_test bytestream_test.cpp)
add_executable(bytestream_simple_test bytestream_simple_test.cpp)

# bytestream remote tests
add_executable(bytestream_remote_local bytestream_remote_local.cpp)
add_executable(bytestream_remote_remote bytestream_remote_remote.cpp)

# memory block allocator test (includes cascade tests)
add_executable(memory_block_allocator_test memory_block_allocator_test.cpp)

# uROS2 tests subdirectory
add_subdirectory(uros2)

# uros2 shared_object tests (commented out - files don't exist yet)
# add_executable(shared_object_test uros2/shared_object/shared_object_test.cpp)
# target_link_libraries(shared_object_test pthread)

# add_executable(memory_allocator_test uros2/shared_object/memory_allocator_test.cpp)
# target_link_libraries(memory_allocator_test pthread)

# add_executable(multithreading_test uros2/shared_object/multithreading_test.cpp)
# target_link_libraries(multithreading_test pthread)

# add_executable(cascaded_pools_test uros2/shared_object/cascaded_pools_test.cpp)
# target_link_libraries(cascaded_pools_test pthread)

# add_executable(realistic_scenario_test uros2/shared_object/realistic_scenario_test.cpp)
# target_link_libraries(realistic_scenario_test pthread)

# add_executable(detailed_scenario_test uros2/shared_object/detailed_scenario_test.cpp)
# target_link_libraries(detailed_scenario_test pthread)

# add_executable(shared_object_inheritance_test uros2/shared_object/shared_object_inheritance_test.cpp)
# target_link_libraries(shared_object_inheritance_test pthread)

# add_executable(alignment_test uros2/shared_object/alignment_test.cpp)
# target_link_libraries(alignment_test pthread)

# add_executable(alignment_extreme_test uros2/shared_object/alignment_extreme_test.cpp)
# target_link_libraries(alignment_extreme_test pthread)

# Negative test - should fail to compile due to static_assert
# Uncomment to verify static_assert works:
# add_executable(shared_object_inheritance_negative_test uros2/shared_object/shared_object_inheritance_negative_test.cpp)
# target_link_libraries(shared_object_inheritance_negative_test pthread)





