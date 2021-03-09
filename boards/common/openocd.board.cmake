# SPDX-License-Identifier: Apache-2.0

board_set_flasher_ifnset(openocd)
board_set_debugger_ifnset(openocd)

# "load_image" or "flash write_image erase"?
if(CONFIG_X86 OR CONFIG_ARC)
  set_ifndef(OPENOCD_USE_LOAD_IMAGE YES)
endif()
if(OPENOCD_USE_LOAD_IMAGE)
  set_ifndef(OPENOCD_FLASH load_image)
else()
  set_ifndef(OPENOCD_FLASH "flash write_image erase")
endif()

set(OPENOCD_CMD_LOAD_DEFAULT "${OPENOCD_FLASH}")
set(OPENOCD_CMD_VERIFY_DEFAULT "verify_image")

# Cortex-M-specific halt debug enable bit clearing
if(CONFIG_CPU_CORTEX_M)
set(OPENOCD_CMD_HALT_DEBUG_EN_CLEAR "mww 0xE000EDF0 0xA05F0000")
endif()

board_finalize_runner_args(openocd
  --cmd-load "${OPENOCD_CMD_LOAD_DEFAULT}"
  --cmd-verify "${OPENOCD_CMD_VERIFY_DEFAULT}"
  --cmd-halt-debug-en-clear "${OPENOCD_CMD_HALT_DEBUG_EN_CLEAR}"
  )
