# SPDX-License-Identifier: Apache-2.0

zephyr_sources_ifdef(CONFIG_BT_H4       h4.c)
zephyr_sources_ifdef(CONFIG_BT_H5       h5.c)
zephyr_sources_ifdef(CONFIG_BT_SPI      spi.c)
zephyr_sources_ifdef(CONFIG_BT_NRF_OPEN_AMP   open_amp_nrf.c)
zephyr_sources_ifdef(CONFIG_BT_STM32_IPM      ipm_stm32wb.c)
zephyr_sources_ifdef(CONFIG_BT_USERCHAN userchan.c)
