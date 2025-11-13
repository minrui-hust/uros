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





