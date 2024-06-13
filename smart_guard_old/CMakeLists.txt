cmake_minimum_required(VERSION 3.22)
project(SmartGuard C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GPIOD REQUIRED libgpiod)
pkg_check_modules(PNG REQUIRED libpng)
pkg_check_modules(V4L2 REQUIRED libv4l2)

add_executable(smart_guard_client client.c client.h packet.h image.c image.h)
add_executable(smart_guard_server server.c server.h packet.h)
add_executable(smart_guard_server_test server_test.c
        server_test.h)

target_link_libraries(smart_guard_client ${GPIOD_LIBRARIES} ${PNG_LIBRARIES} ${V4L2_LIBRARIES})
target_include_directories(smart_guard_client PRIVATE ${GPIOD_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS} ${V4L2_INCLUDE_DIRS})

add_test(SERVER_COMMUNICATION_TEST COMMAND smart_guard_server_test)
file(COPY test_image.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR})