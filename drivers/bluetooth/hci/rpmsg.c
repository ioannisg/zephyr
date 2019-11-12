/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <ipm.h>
#include <sys/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_driver.h>

#include <openamp/open_amp.h>
#include <metal/sys.h>
#include <metal/device.h>
#include <metal/alloc.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#define LOG_MODULE_NAME bt_hci_driver
#include "common/log.h"

#define RPMSG_CMD 0x01
#define RPMSG_ACL 0x02
#define RPMSG_SCO 0x03
#define RPMSG_EVT 0x04

static void bt_rpmsg_rx(u8_t *data, size_t len);

static K_SEM_DEFINE(sync_sem, 0, 1);

static struct device *ipm_tx_handle;
static struct device *ipm_rx_handle;

/* Configuration defines */

#define SHM_START_ADDR      (0x20010000 + 0x400)
#define SHM_SIZE            0x7c00
#define SHM_DEVICE_NAME     "sram0.shm"

#define VRING_COUNT         2
#define VRING_TX_ADDRESS    (SHM_START_ADDR + SHM_SIZE - 0x400)
#define VRING_RX_ADDRESS    (VRING_TX_ADDRESS - 0x400)
#define VRING_ALIGNMENT     4
#define VRING_SIZE          16

#define VDEV_STATUS_ADDR    0x20010000

/* End of configuration defines */

static metal_phys_addr_t shm_physmap[] = { SHM_START_ADDR };
static struct metal_device shm_device = {
	.name = SHM_DEVICE_NAME,
	.bus = NULL,
	.num_regions = 1,
	.regions = {
		{
			.virt       = (void *) SHM_START_ADDR,
			.physmap    = shm_physmap,
			.size       = SHM_SIZE,
			.page_shift = 0xffffffff,
			.page_mask  = 0xffffffff,
			.mem_flags  = 0,
			.ops        = { NULL },
		},
	},
	.node = { NULL },
	.irq_num = 0,
	.irq_info = NULL
};

static struct virtqueue *vq[2];
static struct rpmsg_endpoint ep;

static unsigned char virtio_get_status(struct virtio_device *vdev)
{
	return VIRTIO_CONFIG_STATUS_DRIVER_OK;
}

static void virtio_set_status(struct virtio_device *vdev, unsigned char status)
{
	sys_write8(status, VDEV_STATUS_ADDR);
}

static u32_t virtio_get_features(struct virtio_device *vdev)
{
	return BIT(VIRTIO_RPMSG_F_NS);
}

static void virtio_set_features(struct virtio_device *vdev, u32_t features)
{
	/* No need for implementation */
}

static void virtio_notify(struct virtqueue *vq)
{
	int status;

	status = ipm_send(ipm_tx_handle, 0, 0, NULL, 0);
	if (status != 0) {
		BT_ERR("ipm_send failed to notify: %d", status);
	}
}

const struct virtio_dispatch dispatch = {
	.get_status = virtio_get_status,
	.set_status = virtio_set_status,
	.get_features = virtio_get_features,
	.set_features = virtio_set_features,
	.notify = virtio_notify,
};

static void ipm_callback(void *context, u32_t id, volatile void *data)
{
	BT_DBG("Got callback of id %u", id);
	virtqueue_notification(vq[0]);
}

int endpoint_cb(struct rpmsg_endpoint *ept, void *data, size_t len,
	u32_t src, void *priv)
{
	BT_DBG("Received message of %u bytes.", len);
	BT_HEXDUMP_DBG((uint8_t *)data, len, "Data:");

	bt_rpmsg_rx(data, len);

	return RPMSG_SUCCESS;
}

static void rpmsg_service_unbind(struct rpmsg_endpoint *ep)
{
	rpmsg_destroy_ept(ep);
}

void ns_bind_cb(struct rpmsg_device *rdev, const char *name, u32_t dest)
{
	(void)rpmsg_create_ept(&ep,
				rdev,
				name,
				RPMSG_ADDR_ANY,
				dest,
				endpoint_cb,
				rpmsg_service_unbind);

	k_sem_give(&sync_sem);
}

static int bt_rpmsg_init_internal(void)
{
	int err;
	struct metal_init_params metal_params = METAL_INIT_DEFAULTS;

	static struct virtio_vring_info     rvrings[2];
	static struct rpmsg_virtio_shm_pool shpool;
	static struct virtio_device         vdev;
	static struct rpmsg_virtio_device   rvdev;
	static struct metal_io_region       *io;
	static struct metal_device          *device;

	/* Libmetal setup */
	err = metal_init(&metal_params);
	if (err) {
		BT_ERR("metal_init: failed - error code %d", err);
		return err;
	}

	err = metal_register_generic_device(&shm_device);
	if (err) {
		BT_ERR("Couldn't register shared memory device: %d", err);
		return err;
	}

	err = metal_device_open("generic", SHM_DEVICE_NAME, &device);
	if (err) {
		BT_ERR("metal_device_open failed: %d", err);
		return err;
	}

	io = metal_device_io_region(device, 0);
	if (!io) {
		BT_ERR("metal_device_io_region failed to get region");
		return -ENODEV;
	}

	/* IPM setup */
	ipm_tx_handle = device_get_binding("IPM_0");
	if (!ipm_tx_handle) {
		BT_ERR("Could not get TX IPM device handle");
		return -ENODEV;
	}

	ipm_rx_handle = device_get_binding("IPM_1");
	if (!ipm_rx_handle) {
		BT_ERR("Could not get RX IPM device handle");
		return -ENODEV;
	}

	ipm_register_callback(ipm_rx_handle, ipm_callback, NULL);

	/* Virtqueue setup */
	vq[0] = virtqueue_allocate(VRING_SIZE);
	if (!vq[0]) {
		BT_ERR("virtqueue_allocate failed to alloc vq[0]");
		return -ENOMEM;
	}

	vq[1] = virtqueue_allocate(VRING_SIZE);
	if (!vq[1]) {
		BT_ERR("virtqueue_allocate failed to alloc vq[1]");
		return -ENOMEM;
	}

	rvrings[0].io = io;
	rvrings[0].info.vaddr = (void *)VRING_TX_ADDRESS;
	rvrings[0].info.num_descs = VRING_SIZE;
	rvrings[0].info.align = VRING_ALIGNMENT;
	rvrings[0].vq = vq[0];

	rvrings[1].io = io;
	rvrings[1].info.vaddr = (void *)VRING_RX_ADDRESS;
	rvrings[1].info.num_descs = VRING_SIZE;
	rvrings[1].info.align = VRING_ALIGNMENT;
	rvrings[1].vq = vq[1];

	vdev.role = RPMSG_MASTER;
	vdev.vrings_num = VRING_COUNT;
	vdev.func = &dispatch;
	vdev.vrings_info = &rvrings[0];

	rpmsg_virtio_init_shm_pool(&shpool, (void *)SHM_START_ADDR, SHM_SIZE);
	err = rpmsg_init_vdev(&rvdev, &vdev, ns_bind_cb, io, &shpool);
	if (err) {
		BT_ERR("rpmsg_init_vdev failed %d", err);
		return err;
	}

	/* Since we are using name service, we need to wait for a response
	 * from NS setup and than we need to process it
	 */
	virtqueue_notification(vq[0]);

	/* Wait til nameservice ep is setup */
	k_sem_take(&sync_sem, K_FOREVER);

	return 0;
}

static inline void bt_buf_set_prio(struct net_buf *buf, bool prio)
{
	*((u8_t *)net_buf_user_data(buf) + 1) = prio;
}

static inline bool bt_buf_get_prio(struct net_buf *buf)
{
	return (bool)(*((u8_t *)net_buf_user_data(buf) + 1));
}

static struct net_buf *bt_rpmsg_evt_recv(u8_t *data, size_t remaining)
{
	struct bt_hci_evt_hdr hdr;
	struct net_buf *buf;

	if (remaining < sizeof(hdr)) {
		BT_ERR("Not enought data for event header");
		return NULL;
	}

	memcpy((void *)&hdr, data, sizeof(hdr));
	data += sizeof(hdr);
	remaining -= sizeof(hdr);

	if (remaining != hdr.len) {
		BT_ERR("Event payload length is not correct");
		return NULL;
	}
	remaining = hdr.len;
	BT_DBG("len %u", hdr.len);

	buf = bt_buf_get_evt(hdr.evt, false, K_NO_WAIT);
	if (!buf) {
		BT_ERR("No available event buffers!");
		return buf;
	}

	net_buf_add_mem(buf, &hdr, sizeof(hdr));
	bt_buf_set_prio(buf, bt_hci_evt_is_prio(hdr.evt));

	net_buf_add_mem(buf, data, remaining);

	return buf;
}

static struct net_buf *bt_rpmsg_acl_recv(u8_t *data, size_t remaining)
{
	struct bt_hci_acl_hdr hdr;
	struct net_buf *buf;

	if (remaining < sizeof(hdr)) {
		BT_ERR("Not enought data for ACL header");
		return NULL;
	}

	buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_NO_WAIT);
	if (buf) {
		memcpy((void *)&hdr, data, sizeof(hdr));
		data += sizeof(hdr);
		remaining -= sizeof(hdr);

		net_buf_add_mem(buf, &hdr, sizeof(hdr));
		bt_buf_set_prio(buf, false);
	} else {
		BT_ERR("No available ACL buffers!");
	}

	if (remaining != hdr.len) {
		BT_ERR("ACL payload length is not correct");
		return NULL;
	}
	remaining = sys_le16_to_cpu(hdr.len);

	BT_DBG("len %u", remaining);
	net_buf_add_mem(buf, data, remaining);

	return buf;
}

static void bt_rpmsg_rx(u8_t *data, size_t len)
{
	u8_t pkt_indicator;
	struct net_buf *buf = NULL;
	size_t remaining = len;

	BT_HEXDUMP_DBG(data, len, "RPMsg data:");

	pkt_indicator = *data++;
	remaining -= sizeof(pkt_indicator);

	switch (pkt_indicator) {
	case RPMSG_EVT:
		buf = bt_rpmsg_evt_recv(data, remaining);
		break;

	case RPMSG_ACL:
		buf = bt_rpmsg_acl_recv(data, remaining);
		break;

	default:
		BT_ERR("Unknown HCI type %u", pkt_indicator);
		return;
	}

	if (buf) {
		BT_DBG("Calling bt_recv(%p)", buf);
		if (bt_buf_get_prio(buf)) {
			bt_recv_prio(buf);
		} else {
			bt_recv(buf);
		}

		BT_HEXDUMP_DBG(buf->data, buf->len, "RX buf payload:");
	}
}

static int bt_rpmsg_send(struct net_buf *buf)
{
	u8_t pkt_indicator;

	BT_DBG("buf %p type %u len %u", buf, bt_buf_get_type(buf), buf->len);

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_ACL_OUT:
		pkt_indicator = RPMSG_ACL;
		break;
	case BT_BUF_CMD:
		pkt_indicator = RPMSG_CMD;
		break;
	default:
		BT_ERR("Unknown type %u", bt_buf_get_type(buf));
		goto done;
	}
	net_buf_push_u8(buf, pkt_indicator);

	BT_HEXDUMP_DBG(buf->data, buf->len, "Final HCI buffer:");
	rpmsg_send(&ep, buf->data, buf->len);

done:
	net_buf_unref(buf);
	return 0;
}

static int bt_rpmsg_open(void)
{
	int err;

	BT_DBG("");

	err = bt_rpmsg_init_internal();
	if (err) {
		return err;
	}

	return 0;
}

static const struct bt_hci_driver drv = {
	.name		= "RPMsg",
	.open		= bt_rpmsg_open,
	.send		= bt_rpmsg_send,
};

static int bt_rpmsg_init(struct device *unused)
{
	ARG_UNUSED(unused);

	int err;

	err = bt_hci_driver_register(&drv);
	if (err) {
		return err;
	}

	return 0;
}

SYS_INIT(bt_rpmsg_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
