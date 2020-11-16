# Add "tfm" as an external project via the TF-M module's cmake file
if("${TFM_TARGET_PLATFORM}" STREQUAL "stm/nucleo_l552ze_q")
   trusted_firmware_build(BINARY_DIR ${CMAKE_BINARY_DIR}/tfm
                          BOARD ${TFM_TARGET_PLATFORM}
                          IPC
                          ISOLATION_LEVEL 2
                          REGRESSION
                          ${TFM_BL2_ARGUMENT}
   )
else()
   trusted_firmware_build(BINARY_DIR ${CMAKE_BINARY_DIR}/tfm
                          BOARD ${TFM_TARGET_PLATFORM}
                          IPC
                          REGRESSION
                          ${TFM_BL2_ARGUMENT}
  )
endif()

project(tfm_ipc)

# Set "tfm" as a dependency
add_dependencies(app tfm)

target_sources(app PRIVATE src/main.c)

# Include TF-M secure service source files
target_link_libraries(app PRIVATE tfm_api)

# Link in veneer function locations
target_link_libraries(${ZEPHYR_CURRENT_LIBRARY} PRIVATE ${VENEERS_FILE})
