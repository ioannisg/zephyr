# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BOARD_NRF5340_PCA10095_CPU0 OR CONFIG_BOARD_NRF5340_PCA10095_CPU0NS)
board_runner_args(nrfjprog "--nrf-family=NRF53"-tool-opt=--coprocessor CP_APPLICATION")
board_runner_args(jlink "--device=cortex-m33" "--speed=4000")
endif()

if(CONFIG_BOARD_NRF5340_PCA10095_CPU1)
board_runner_args(nrfjprog "--nrf-family=NRF53" "--tool-opt=--coprocessor CP_NETWORK")
endif()

include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
