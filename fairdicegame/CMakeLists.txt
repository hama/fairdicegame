add_executable(${CMAKE_PROJECT_NAME}.wasm ${CMAKE_CURRENT_SOURCE_DIR}/src/${CMAKE_PROJECT_NAME}.cpp)
target_include_directories(${CMAKE_PROJECT_NAME}.wasm
   PUBLIC 
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(${CMAKE_PROJECT_NAME}.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_target(${CMAKE_PROJECT_NAME}.abi ALL)

set(ABI_FILE "${CMAKE_CURRENT_SOURCE_DIR}/abi/${CMAKE_PROJECT_NAME}.abi")

add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}.abi
    COMMAND eosiocpp -g  ${ABI_FILE}   "${CMAKE_CURRENT_SOURCE_DIR}/include/${CMAKE_PROJECT_NAME}.hpp"
)

if(EXISTS ${ABI_FILE})  
configure_file(${ABI_FILE}  "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)
endif()

#install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${WASM_ROOT}/eosio.wasmsdk/include)