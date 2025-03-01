/* btp_core.c - Bluetooth Core service */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 * Copyright (c) 2023 Codecoup
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/atomic.h>
#include <zephyr/types.h>
#include <string.h>

#include <zephyr/toolchain.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/net/buf.h>

#include <hci_core.h>

#include <zephyr/logging/log.h>
#define LOG_MODULE_NAME bttester_core
LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_BTTESTER_LOG_LEVEL);

#include "btp/btp.h"

static ATOMIC_DEFINE(registered_services, BTP_SERVICE_ID_MAX);

static uint8_t supported_commands(const void *cmd, uint16_t cmd_len,
				  void *rsp, uint16_t *rsp_len)
{
	struct btp_core_read_supported_commands_rp *rp = rsp;

	tester_set_bit(rp->data, BTP_CORE_READ_SUPPORTED_COMMANDS);
	tester_set_bit(rp->data, BTP_CORE_READ_SUPPORTED_SERVICES);
	tester_set_bit(rp->data, BTP_CORE_REGISTER_SERVICE);
	tester_set_bit(rp->data, BTP_CORE_UNREGISTER_SERVICE);

	*rsp_len = sizeof(*rp) + 1;

	return BTP_STATUS_SUCCESS;
}

static uint8_t supported_services(const void *cmd, uint16_t cmd_len,
				  void *rsp, uint16_t *rsp_len)
{
	struct btp_core_read_supported_services_rp *rp = rsp;

	/* octet 0 */
	tester_set_bit(rp->data, BTP_SERVICE_ID_CORE);
	tester_set_bit(rp->data, BTP_SERVICE_ID_GAP);
	tester_set_bit(rp->data, BTP_SERVICE_ID_GATT);
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	tester_set_bit(rp->data, BTP_SERVICE_ID_L2CAP);
#endif /* CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */
#if defined(CONFIG_BT_MESH)
	tester_set_bit(rp->data, BTP_SERVICE_ID_MESH);
#endif /* CONFIG_BT_MESH */

	/* octet 1 */
#if defined(CONFIG_BT_VCP_VOL_REND)
	tester_set_bit(rp->data, BTP_SERVICE_ID_VCS);
#endif /* CONFIG_BT_VCP_VOL_REND */
#if defined(CONFIG_BT_IAS) || defined(CONFIG_BT_IAS_CLIENT)
	tester_set_bit(rp->data, BTP_SERVICE_ID_IAS);
#endif /* CONFIG_BT_IAS */
#if defined(CONFIG_BT_AICS) || defined(CONFIG_BT_AICS_CLIENT)
	tester_set_bit(rp->data, BTP_SERVICE_ID_AICS);
#endif /*CONFIG_BT_AICS */
#if defined(CONFIG_BT_VOCS) || defined(CONFIG_BT_VOCS_CLIENT)
	tester_set_bit(rp->data, BTP_SERVICE_ID_VOCS);
#endif /* CONFIG_BT_VOCS */
#if defined(CONFIG_BT_HAS) || defined(CONFIG_BT_HAS_CLIENT)
	tester_set_bit(rp->data, BTP_SERVICE_ID_HAS);
#endif /* CONFIG_BT_HAS */
#if defined(CONFIG_BT_CSIP_SET_MEMBER)
	tester_set_bit(rp->data, BTP_SERVICE_ID_CSIS);
#endif /* CONFIG_BT_CSIP_SET_MEMBER */
#if defined(CONFIG_BT_MICP_MIC_DEV) || defined(CONFIG_BT_MICP_MIC_CTLR)
	tester_set_bit(rp->data, BTP_SERVICE_ID_MICP);
#endif /* CONFIG_BT_MICP_MIC_DEV */

	*rsp_len = sizeof(*rp) + 2;

	return BTP_STATUS_SUCCESS;
}

static uint8_t register_service(const void *cmd, uint16_t cmd_len,
				void *rsp, uint16_t *rsp_len)
{
	const struct btp_core_register_service_cmd *cp = cmd;
	uint8_t status;

	/* invalid service */
	if ((cp->id == BTP_SERVICE_ID_CORE) || (cp->id > BTP_SERVICE_ID_MAX)) {
		return BTP_STATUS_FAILED;
	}

	/* already registered */
	if (atomic_test_bit(registered_services, cp->id)) {
		return BTP_STATUS_FAILED;
	}

	switch (cp->id) {
	case BTP_SERVICE_ID_GAP:
		status = tester_init_gap();
		break;
	case BTP_SERVICE_ID_GATT:
		status = tester_init_gatt();
		break;
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	case BTP_SERVICE_ID_L2CAP:
		status = tester_init_l2cap();
#endif /* CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */
		break;
#if defined(CONFIG_BT_MESH)
	case BTP_SERVICE_ID_MESH:
		status = tester_init_mesh();
		break;
#endif /* CONFIG_BT_MESH */
#if defined(CONFIG_BT_VCP_VOL_REND)
	case BTP_SERVICE_ID_VCS:
		status = tester_init_vcs();
		break;
	case BTP_SERVICE_ID_VOCS:
		status = tester_init_vocs();
		break;
	case BTP_SERVICE_ID_AICS:
		status = tester_init_aics();
		break;
#endif /* CONFIG_BT_VCP_VOL_REND */
#if defined(CONFIG_BT_IAS)
	case BTP_SERVICE_ID_IAS:
		status = tester_init_ias();
		break;
#endif /* CONFIG_BT_IAS */
#if defined(CONFIG_BT_BAP_UNICAST_CLIENT) || defined(CONFIG_BT_BAP_UNICAST_SERVER)
	case BTP_SERVICE_ID_PACS:
		status = tester_init_pacs();
		break;
	case BTP_SERVICE_ID_ASCS:
		status = tester_init_ascs();
		break;
	case BTP_SERVICE_ID_BAP:
		status = tester_init_bap();
		break;
#endif /* CONFIG_BT_BAP_UNICAST_CLIENT or CONFIG_BT_BAP_UNICAST_SERVER */
#if defined(CONFIG_BT_MICP_MIC_DEV) || defined(CONFIG_BT_MICP_MIC_CTLR)
	case BTP_SERVICE_ID_MICP:
		status = tester_init_micp();
		break;
	case BTP_SERVICE_ID_MICS:
		status = tester_init_mics();
		break;
#endif /* CONFIG_BT_MICP_MIC_DEV or CONFIG_BT_MICP_MIC_CTLR */
#if defined(CONFIG_BT_HAS)
	case BTP_SERVICE_ID_HAS:
		status = tester_init_has();
		break;
#endif /* CONFIG_BT_HAS */
#if defined(CONFIG_BT_CSIP_SET_MEMBER)
	case BTP_SERVICE_ID_CSIS:
		status = tester_init_csis();
		break;
#endif /* CONFIG_BT_CSIP_SET_MEMBER */
	default:
		LOG_WRN("unknown id: 0x%02x", cp->id);
		status = BTP_STATUS_FAILED;
		break;
	}

	if (status == BTP_STATUS_SUCCESS) {
		atomic_set_bit(registered_services, cp->id);
	}

	return status;
}

static uint8_t unregister_service(const void *cmd, uint16_t cmd_len,
				  void *rsp, uint16_t *rsp_len)
{
	const struct btp_core_unregister_service_cmd *cp = cmd;
	uint8_t status;

	/* invalid service ID */
	if ((cp->id == BTP_SERVICE_ID_CORE) || (cp->id > BTP_SERVICE_ID_MAX)) {
		return BTP_STATUS_FAILED;
	}

	/* not registered */
	if (!atomic_test_bit(registered_services, cp->id)) {
		return BTP_STATUS_FAILED;
	}

	switch (cp->id) {
	case BTP_SERVICE_ID_GAP:
		status = tester_unregister_gap();
		break;
	case BTP_SERVICE_ID_GATT:
		status = tester_unregister_gatt();
		break;
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	case BTP_SERVICE_ID_L2CAP:
		status = tester_unregister_l2cap();
		break;
#endif /* CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */
#if defined(CONFIG_BT_MESH)
	case BTP_SERVICE_ID_MESH:
		status = tester_unregister_mesh();
		break;
#endif /* CONFIG_BT_MESH */
#if defined(CONFIG_BT_VCP_VOL_REND)
	case BTP_SERVICE_ID_VCS:
		status = tester_unregister_vcs();
		break;
	case BTP_SERVICE_ID_AICS:
		status = tester_unregister_aics();
		break;
	case BTP_SERVICE_ID_VOCS:
		status = tester_unregister_vocs();
		break;
#endif /* CONFIG_BT_VCP_VOL_REND */
#if defined(CONFIG_BT_IAS)
	case BTP_SERVICE_ID_IAS:
		status = tester_unregister_ias();
		break;
#endif /* CONFIG_BT_IAS */
#if defined(CONFIG_BT_BAP_UNICAST_CLIENT) || defined(CONFIG_BT_BAP_UNICAST_SERVER)
	case BTP_SERVICE_ID_PACS:
		status = tester_unregister_pacs();
		break;
	case BTP_SERVICE_ID_ASCS:
		status = tester_unregister_ascs();
		break;
	case BTP_SERVICE_ID_BAP:
		status = tester_unregister_bap();
		break;
		case BTP_SERVICE_ID_MICP:
		status = tester_unregister_micp();
		break;
#endif /* CONFIG_BT_BAP_UNICAST_CLIENT or CONFIG_BT_BAP_UNICAST_SERVER */
#if defined(CONFIG_BT_HAS)
	case BTP_SERVICE_ID_HAS:
		status = tester_unregister_has();
		break;
#endif /* CONFIG_BT_HAS */
#if defined(CONFIG_BT_CSIP_SET_MEMBER)
	case BTP_SERVICE_ID_CSIS:
		status = tester_unregister_csis();
		break;
#endif /* CONFIG_BT_CSIP_SET_MEMBER */
	default:
		LOG_WRN("unknown id: 0x%x", cp->id);
		status = BTP_STATUS_FAILED;
		break;
	}

	if (status == BTP_STATUS_SUCCESS) {
		atomic_clear_bit(registered_services, cp->id);
	}

	return BTP_STATUS_FAILED;
}

static const struct btp_handler handlers[] = {
	{
		.opcode = BTP_CORE_READ_SUPPORTED_COMMANDS,
		.index = BTP_INDEX_NONE,
		.expect_len = 0,
		.func = supported_commands,
	},
	{
		.opcode = BTP_CORE_READ_SUPPORTED_SERVICES,
		.index = BTP_INDEX_NONE,
		.expect_len = 0,
		.func = supported_services,
	},
	{
		.opcode = BTP_CORE_REGISTER_SERVICE,
		.index = BTP_INDEX_NONE,
		.expect_len = sizeof(struct btp_core_register_service_cmd),
		.func = register_service,
	},
	{
		.opcode = BTP_CORE_UNREGISTER_SERVICE,
		.index = BTP_INDEX_NONE,
		.expect_len = sizeof(struct btp_core_unregister_service_cmd),
		.func = unregister_service,
	},
};

void tester_init_core(void)
{
	tester_register_command_handlers(BTP_SERVICE_ID_CORE, handlers,
					 ARRAY_SIZE(handlers));
	atomic_set_bit(registered_services, BTP_SERVICE_ID_CORE);
}
