cmake_minimum_required(VERSION 3.16)
project(AutonomousAdministration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Packages
find_package(CURL REQUIRED)
find_package(PNG REQUIRED)

# nlohmann_json is header-only. We can locate its headers.
find_path(NLOHMANN_JSON_INCLUDE_DIR NAMES nlohmann/json.hpp)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(WARNING "nlohmann/json.hpp not found in default paths")
endif()

# Find libzip manually to bypass broken cmake package
find_library(ZIP_LIBRARY NAMES zip)
find_path(ZIP_INCLUDE_DIR NAMES zip.h)

# Find libssh manually
find_library(SSH_LIBRARY NAMES ssh)
find_path(SSH_INCLUDE_DIR NAMES libssh/libssh.h)

add_executable(AutonomousAdministration
    main.cpp
    IWorker.cpp
    ProjectScanner.cpp
    ProjectCoordinator.cpp
    RequirementsGatherer.cpp
    APIRequest.cpp
    ISubProject.cpp
    SubResearchProject.cpp
)

# Include directories
if(NLOHMANN_JSON_INCLUDE_DIR)
    target_include_directories(AutonomousAdministration PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
endif()
if(ZIP_INCLUDE_DIR)
    target_include_directories(AutonomousAdministration PRIVATE ${ZIP_INCLUDE_DIR})
endif()
if(SSH_INCLUDE_DIR)
    target_include_directories(AutonomousAdministration PRIVATE ${SSH_INCLUDE_DIR})
endif()

# Link libraries
target_link_libraries(AutonomousAdministration PRIVATE
    ${CURL_LIBRARIES}
    ${PNG_LIBRARIES}
)

if(ZIP_LIBRARY)
    target_link_libraries(AutonomousAdministration PRIVATE ${ZIP_LIBRARY})
else()
    message(FATAL_ERROR "libzip not found!")
endif()

if(SSH_LIBRARY)
    target_link_libraries(AutonomousAdministration PRIVATE ${SSH_LIBRARY})
else()
    message(FATAL_ERROR "libssh not found!")
endif()
