/*
 * Copyright (c) 2019, Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <nrfx_ipc.h>

#define NRFX_IPC_ID_MAX_VALUE	 IPC_CONF_NUM

/*
 * Group IPC signals, events and channels into message channels.
 * Message channels are one-way connections between cores.
 *
 * For example Message Channel 0 is configured as TX on core 0
 * and as RX on core 1:
 *
 * [C0]			    [C1]
 * SIGNAL0 -> CHANNEL0 -> EVENT0
 *
 * Message Channel 1 is configured as RX on core 0 and as TX
 * on core 1:
 * [C0]			    [C1]
 * EVENT1 <- CHANNEL1 <- SIGNAL1
 */

#define IPC_EVENT_BIT(idx) \
	((IS_ENABLED(CONFIG_IPM_MSG_CH_##idx##_RX)) << idx)

#define IPC_EVENT_BITS		\
	(			\
	 IPC_EVENT_BIT(0)  |	\
	 IPC_EVENT_BIT(1)  |	\
	 IPC_EVENT_BIT(2)  |	\
	 IPC_EVENT_BIT(3)  |	\
	 IPC_EVENT_BIT(4)  |	\
	 IPC_EVENT_BIT(5)  |	\
	 IPC_EVENT_BIT(6)  |	\
	 IPC_EVENT_BIT(7)  |	\
	 IPC_EVENT_BIT(8)  |	\
	 IPC_EVENT_BIT(9)  |	\
	 IPC_EVENT_BIT(10) |	\
	 IPC_EVENT_BIT(11) |	\
	 IPC_EVENT_BIT(12) |	\
	 IPC_EVENT_BIT(13) |	\
	 IPC_EVENT_BIT(14) |	\
	 IPC_EVENT_BIT(15)	\
	)

static const nrfx_ipc_config_t ipc_cfg = {
	.send_task_config = {
		[0] = (1UL << 0),
		[1] = (1UL << 1),
		[2] = (1UL << 2),
		[3] = (1UL << 3),
		[4] = (1UL << 4),
		[5] = (1UL << 5),
		[6] = (1UL << 6),
		[7] = (1UL << 7),
		[8] = (1UL << 8),
		[9] = (1UL << 9),
		[10] = (1UL << 10),
		[11] = (1UL << 11),
		[12] = (1UL << 12),
		[13] = (1UL << 13),
		[14] = (1UL << 14),
		[15] = (1UL << 15),
	},
	.receive_event_config = {
		[0] = (1UL << 0),
		[1] = (1UL << 1),
		[2] = (1UL << 2),
		[3] = (1UL << 3),
		[4] = (1UL << 4),
		[5] = (1UL << 5),
		[6] = (1UL << 6),
		[7] = (1UL << 7),
		[8] = (1UL << 8),
		[9] = (1UL << 9),
		[10] = (1UL << 10),
		[11] = (1UL << 11),
		[12] = (1UL << 12),
		[13] = (1UL << 13),
		[14] = (1UL << 14),
		[15] = (1UL << 15),
	},
	.receive_events_enabled = IPC_EVENT_BITS,
};
