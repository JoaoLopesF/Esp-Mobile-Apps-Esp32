/* ***********
 * Project   : util - Utilities to esp-idf
 * Programmer: Joao Lopes
 * Module    : ble_uart_server - BLE UART server to esp-idf
 * Comments  : Based in pcbreflux samples
 * Versions  :
 * ------- 	-------- 	-------------------------
 * 0.1.0 	01/08/18 	First version
 * 0.3.0  	23/08/18	Adjustments to allow sizes of BLE > 255
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

//#include "soc.h"

#include "sdkconfig.h"

#include "ble_uart_server.h"

////// Variables

static const char* TAG = "ble_uart_server";							// Log tag

static void (*mCallbackConnection)();								// Callback for connection/disconnection
static void (*mCallbackMTU)();										// Callback for MTU change detect
static void (*mCallbackReceivedData) (char *data, uint16_t size); 	// Callback for receive data
static esp_gatt_if_t mGatts_if = ESP_GATT_IF_NONE;					// To save gatts_if

static bool mConnected = false;										// Connected ?
static char mDeviceName[30];										// Device name
static uint16_t mMTU = 20;											// MTU of BLE data
static const uint8_t* mMacAddress;										// Mac address

////// Prototypes

// Private

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void gap_event_handler(esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t * param);
static void gatts_event_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);

static void char1_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void char1_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void descr1_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void descr1_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void char2_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void char2_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void descr2_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void descr2_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);

////// Routines based on the pbcreflux example

static uint8_t char1_str[GATTS_CHAR_VAL_LEN_MAX] = { 0x11, 0x22, 0x33 };
static uint8_t char2_str[GATTS_CHAR_VAL_LEN_MAX] = { 0x11, 0x22, 0x33 };
static uint8_t descr1_str[GATTS_CHAR_VAL_LEN_MAX] = { 0x00, 0x00 };
static uint8_t descr2_str[GATTS_CHAR_VAL_LEN_MAX] = "ESP32_BLE_UART";

static esp_attr_value_t gatts_demo_char1_val = { .attr_max_len =
		GATTS_CHAR_VAL_LEN_MAX, .attr_len = sizeof(char1_str), .attr_value =
		char1_str, };

static esp_attr_value_t gatts_demo_char2_val = { .attr_max_len =
		GATTS_CHAR_VAL_LEN_MAX, .attr_len = sizeof(char2_str), .attr_value =
		char2_str, };

static esp_attr_value_t gatts_demo_descr1_val = { .attr_max_len =
		GATTS_CHAR_VAL_LEN_MAX, .attr_len = sizeof(descr1_str), .attr_value =
		descr1_str, };

static esp_attr_value_t gatts_demo_descr2_val = { .attr_max_len =
		GATTS_CHAR_VAL_LEN_MAX, .attr_len = 11, .attr_value = descr2_str, };

#define BLE_SERVICE_UUID_SIZE ESP_UUID_LEN_128

// Add more UUIDs for more then one Service
static uint8_t ble_service_uuid128[BLE_SERVICE_UUID_SIZE] = {
		/* LSB <--------------------------------------------------------------------------------> MSB */
		//first uuid, 16bit, [12],[13] is the value
		0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5,
		0x01, 0x00, 0x40, 0x6E, };

static uint8_t ble_manufacturer[BLE_MANUFACTURER_DATA_LEN] = { 0x12, 0x23, 0x45,
		0x56 };

static uint32_t ble_add_char_pos;

static esp_ble_adv_data_t ble_adv_data = { .set_scan_rsp = false,
		.include_name = true, .include_txpower = true, .min_interval = 0x20,
		.max_interval = 0x40, .appearance = 0x00, .manufacturer_len =
				BLE_MANUFACTURER_DATA_LEN, .p_manufacturer_data =
				(uint8_t *) ble_manufacturer, .service_data_len = 0,
		.p_service_data = NULL, .service_uuid_len = BLE_SERVICE_UUID_SIZE,
		.p_service_uuid = ble_service_uuid128, .flag =
				(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT), };

static esp_ble_adv_params_t ble_adv_params = { .adv_int_min = 0x20,
		.adv_int_max = 0x40, .adv_type = ADV_TYPE_IND, .own_addr_type =
				BLE_ADDR_TYPE_PUBLIC, .channel_map = ADV_CHNL_ALL,
		.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY, };

struct gatts_profile_inst {
	uint16_t gatts_if;
	uint16_t app_id;
	uint16_t conn_id;
	uint16_t service_handle;
	esp_gatt_srvc_id_t service_id;
	uint16_t char_handle;
	esp_bt_uuid_t char_uuid;
	esp_gatt_perm_t perm;
	esp_gatt_char_prop_t property;
	uint16_t descr_handle;
	esp_bt_uuid_t descr_uuid;
};

struct gatts_char_inst {
	esp_bt_uuid_t char_uuid;
	esp_gatt_perm_t char_perm;
	esp_gatt_char_prop_t char_property;
	esp_attr_value_t *char_val;
	esp_attr_control_t *char_control;
	uint16_t char_handle;
	esp_gatts_cb_t char_read_callback;
	esp_gatts_cb_t char_write_callback;
	esp_bt_uuid_t descr_uuid;
	esp_gatt_perm_t descr_perm;
	esp_attr_value_t *descr_val;
	esp_attr_control_t *descr_control;
	uint16_t descr_handle;
	esp_gatts_cb_t descr_read_callback;
	esp_gatts_cb_t descr_write_callback;
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gl_profile = { .gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_char_inst gl_char[GATTS_CHAR_NUM] = { {
		.char_uuid.len = ESP_UUID_LEN_128, // RX
		.char_uuid.uuid.uuid128 = { 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9,
				0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E },
		.char_perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, .char_property =
				ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE
						| ESP_GATT_CHAR_PROP_BIT_NOTIFY, .char_val =
				&gatts_demo_char1_val, .char_control = NULL, .char_handle = 0,
		.char_read_callback = char1_read_handler, .char_write_callback =
				char1_write_handler, .descr_uuid.len = ESP_UUID_LEN_16,
		.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
		.descr_perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, .descr_val =
				&gatts_demo_descr1_val, .descr_control = NULL,
		.descr_handle = 0, .descr_read_callback = descr1_read_handler,
		.descr_write_callback = descr1_write_handler }, {
		.char_uuid.len = ESP_UUID_LEN_128,  // TX
		.char_uuid.uuid.uuid128 = { 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9,
				0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E },
		.char_perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, .char_property =
				ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE
						| ESP_GATT_CHAR_PROP_BIT_NOTIFY, .char_val =
				&gatts_demo_char2_val, .char_control = NULL, .char_handle = 0,
		.char_read_callback = char2_read_handler, .char_write_callback =
				char2_write_handler, .descr_uuid.len = ESP_UUID_LEN_16,
		.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG, // ESP_GATT_UUID_CHAR_DESCRIPTION,
		.descr_perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, .descr_val =
				&gatts_demo_descr2_val, .descr_control = NULL,
		.descr_handle = 0, .descr_read_callback = descr2_read_handler,
		.descr_write_callback = descr2_write_handler } };

static void char1_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	ble_logD("char1_read_handler %d", param->read.handle);

	esp_gatt_rsp_t rsp;
	memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
	rsp.attr_value.handle = param->read.handle;
	if (gl_char[0].char_val != NULL) {
		ble_logD("char1_read_handler char_val %d",gl_char[0].char_val->attr_len);
		rsp.attr_value.len = gl_char[0].char_val->attr_len;
		for (uint32_t pos = 0;
				pos < gl_char[0].char_val->attr_len
						&& pos < gl_char[0].char_val->attr_max_len; pos++) {
			rsp.attr_value.value[pos] = gl_char[0].char_val->attr_value[pos];
		}
	} ble_logD("char1_read_handler esp_gatt_rsp_t");
	esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
			param->read.trans_id, ESP_GATT_OK, &rsp);
}

static void char2_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	ble_logD("char2_read_handler %d", param->read.handle);

	esp_gatt_rsp_t rsp;
	memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
	rsp.attr_value.handle = param->read.handle;
	if (gl_char[1].char_val != NULL) {
		ble_logD("char2_read_handler char_val %d",gl_char[1].char_val->attr_len);
		rsp.attr_value.len = gl_char[1].char_val->attr_len;
		for (uint32_t pos = 0;
				pos < gl_char[1].char_val->attr_len
						&& pos < gl_char[1].char_val->attr_max_len; pos++) {
			rsp.attr_value.value[pos] = gl_char[1].char_val->attr_value[pos];
		}
	} ble_logD("char2_read_handler esp_gatt_rsp_t");
	esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
			param->read.trans_id, ESP_GATT_OK, &rsp);
}

static void descr1_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	ble_logD("descr1_read_handler %d", param->read.handle);

	esp_gatt_rsp_t rsp;
	memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
	rsp.attr_value.handle = param->read.handle;
	if (gl_char[0].descr_val != NULL) {
		ble_logD("descr1_read_handler descr_val %d",gl_char[0].descr_val->attr_len);
		rsp.attr_value.len = gl_char[0].descr_val->attr_len;
		for (uint32_t pos = 0;
				pos < gl_char[0].descr_val->attr_len
						&& pos < gl_char[0].descr_val->attr_max_len; pos++) {
			rsp.attr_value.value[pos] = gl_char[0].descr_val->attr_value[pos];
		}
	} ble_logD("descr1_read_handler esp_gatt_rsp_t ");
	esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
			param->read.trans_id, ESP_GATT_OK, &rsp);
}

static void descr2_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	ble_logD("descr2_read_handler %d", param->read.handle);

	esp_gatt_rsp_t rsp;
	memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
	rsp.attr_value.handle = param->read.handle;
	if (gl_char[0].descr_val != NULL) {
		ble_logD("descr2_read_handler descr_val %d",gl_char[1].descr_val->attr_len);
		rsp.attr_value.len = gl_char[1].descr_val->attr_len;
		for (uint32_t pos = 0;
				pos < gl_char[1].descr_val->attr_len
						&& pos < gl_char[1].descr_val->attr_max_len; pos++) {
			rsp.attr_value.value[pos] = gl_char[1].descr_val->attr_value[pos];
		}
	} ble_logD("descr2_read_handler esp_gatt_rsp_t");
	esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
			param->read.trans_id, ESP_GATT_OK, &rsp);
}

//static  uint16_t notify_conn_id = 0;
//static  esp_gatt_if_t notify_gatts_if = NULL;
//static uint8_t notify_pos=0;
static uint8_t is_notify = 0;

static void char2_notify_handle(esp_gatt_if_t gatts_if, uint16_t conn_id) {
//	if (is_notify==1) {
//		notify_pos='0';
//	}
}

static void char1_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	ble_logD("char1_write_handler %d", param->write.handle);

	if (gl_char[0].char_val != NULL) {
		ble_logD("char1_write_handler char_val %d",param->write.len);
		gl_char[0].char_val->attr_len = param->write.len;
		for (uint32_t pos = 0; pos < param->write.len; pos++) {
			gl_char[0].char_val->attr_value[pos] = param->write.value[pos];
		} ble_logD("char1_write_handler %.*s", gl_char[0].char_val->attr_len, (char*)gl_char[0].char_val->attr_value);
	} ble_logD(" char1_write_handler esp_gatt_rsp_t");
	esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
			param->write.trans_id, ESP_GATT_OK, NULL);

	// Original code changed here !

	// Callback for receive data (after the send_response to works!!)

	if (gl_char[0].char_val != NULL && gl_char[0].char_val->attr_len > 0) { // Read OK ;-)

		// Callback para receive data

		if (mCallbackReceivedData != NULL) {

			mCallbackReceivedData((char*) gl_char[0].char_val->attr_value,
					(uint8_t) gl_char[0].char_val->attr_len);
		}
	}
}

static void char2_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	ble_logD("char2_write_handler %d", param->write.handle);

	if (gl_char[1].char_val != NULL) {
		ble_logD("char2_write_handler char_val %d",param->write.len);
		gl_char[1].char_val->attr_len = param->write.len;
		for (uint32_t pos = 0; pos < param->write.len; pos++) {
			gl_char[1].char_val->attr_value[pos] = param->write.value[pos];
		}
	} ble_logD("char2_write_handler esp_gatt_rsp_t");
	esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
			param->write.trans_id, ESP_GATT_OK, NULL);
}

static void descr1_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	ble_logD("descr1_write_handler %d", param->write.handle);

	if (gl_char[0].descr_val != NULL) {
		ble_logD("descr1_write_handler descr_val %d",param->write.len);
		gl_char[0].descr_val->attr_len = param->write.len;
		for (uint32_t pos = 0; pos < param->write.len; pos++) {
			gl_char[0].descr_val->attr_value[pos] = param->write.value[pos];
		}
	} ble_logD("descr1_write_handler esp_gatt_rsp_t");
	esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
			param->write.trans_id, ESP_GATT_OK, NULL);
}

static void descr2_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {

	// Original code changed here !

	ble_logD("descr2_write_handler %d", param->write.handle);

	if (gl_char[1].descr_val != NULL) {
		ble_logD("descr2_write_handler descr_val %d",param->write.len);
		gl_char[1].descr_val->attr_len = param->write.len;
		for (uint32_t pos = 0; pos < param->write.len; pos++) {
			gl_char[1].descr_val->attr_value[pos] = param->write.value[pos];
		}
		is_notify = gl_char[1].descr_val->attr_value[0];
		ble_logD("descr1_write_handler is_notify %d",is_notify);
	} ble_logD("descr2_write_handler esp_gatt_rsp_t");
	esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
			param->write.trans_id, ESP_GATT_OK, NULL);
}

static void gatts_add_char() {

	ble_logD("gatts_add_char %d", GATTS_CHAR_NUM);
	for (uint32_t pos = 0; pos < GATTS_CHAR_NUM; pos++) {
		if (gl_char[pos].char_handle == 0) {
			ble_logD("ADD pos %d handle %d service %d", pos,gl_char[pos].char_handle,gl_profile.service_handle);
			ble_add_char_pos = pos;
			esp_ble_gatts_add_char(gl_profile.service_handle,
					&gl_char[pos].char_uuid, gl_char[pos].char_perm,
					gl_char[pos].char_property, gl_char[pos].char_val,
					gl_char[pos].char_control);
			break;
		}
	}
}

static void gatts_check_add_char(esp_bt_uuid_t char_uuid, uint16_t attr_handle) {

	ble_logD("gatts_check_add_char %d", attr_handle);
	if (attr_handle != 0) {
		if (char_uuid.len == ESP_UUID_LEN_16) {
			ble_logD("Char UUID16: %x", char_uuid.uuid.uuid16);
		} else if (char_uuid.len == ESP_UUID_LEN_32) {
			ble_logD("Char UUID32: %x", char_uuid.uuid.uuid32);
		} else if (char_uuid.len == ESP_UUID_LEN_128) {
			ble_logD("Char UUID128: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x", char_uuid.uuid.uuid128[0],
					char_uuid.uuid.uuid128[1], char_uuid.uuid.uuid128[2], char_uuid.uuid.uuid128[3],
					char_uuid.uuid.uuid128[4], char_uuid.uuid.uuid128[5], char_uuid.uuid.uuid128[6],
					char_uuid.uuid.uuid128[7], char_uuid.uuid.uuid128[8], char_uuid.uuid.uuid128[9],
					char_uuid.uuid.uuid128[10], char_uuid.uuid.uuid128[11], char_uuid.uuid.uuid128[12],
					char_uuid.uuid.uuid128[13], char_uuid.uuid.uuid128[14], char_uuid.uuid.uuid128[15]);
		} else {
			ble_logE("Char UNKNOWN LEN %d", char_uuid.len);
		}

		ble_logD("FOUND Char pos %d handle %d", ble_add_char_pos,attr_handle);
		gl_char[ble_add_char_pos].char_handle = attr_handle;

		// is there a descriptor to add ?
		if (gl_char[ble_add_char_pos].descr_uuid.len != 0
				&& gl_char[ble_add_char_pos].descr_handle == 0) {
			ble_logD("ADD Descr pos %d handle %d service %d", ble_add_char_pos,gl_char[ble_add_char_pos].descr_handle,gl_profile.service_handle);
			esp_ble_gatts_add_char_descr(gl_profile.service_handle,
					&gl_char[ble_add_char_pos].descr_uuid,
					gl_char[ble_add_char_pos].descr_perm,
					gl_char[ble_add_char_pos].descr_val,
					gl_char[ble_add_char_pos].descr_control);
		} else {
			gatts_add_char();
		}
	}
}

static void gatts_check_add_descr(esp_bt_uuid_t descr_uuid,
		uint16_t attr_handle) {

	ble_logD("gatts_check_add_descr %d", attr_handle);
	if (attr_handle != 0) {
		if (descr_uuid.len == ESP_UUID_LEN_16) {
			ble_logD("Char UUID16: %x", descr_uuid.uuid.uuid16);
		} else if (descr_uuid.len == ESP_UUID_LEN_32) {
			ble_logD("Char UUID32: %x", descr_uuid.uuid.uuid32);
		} else if (descr_uuid.len == ESP_UUID_LEN_128) {
			ble_logD("Char UUID128: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x", descr_uuid.uuid.uuid128[0],
					descr_uuid.uuid.uuid128[1], descr_uuid.uuid.uuid128[2], descr_uuid.uuid.uuid128[3],
					descr_uuid.uuid.uuid128[4], descr_uuid.uuid.uuid128[5], descr_uuid.uuid.uuid128[6],
					descr_uuid.uuid.uuid128[7], descr_uuid.uuid.uuid128[8], descr_uuid.uuid.uuid128[9],
					descr_uuid.uuid.uuid128[10], descr_uuid.uuid.uuid128[11], descr_uuid.uuid.uuid128[12],
					descr_uuid.uuid.uuid128[13], descr_uuid.uuid.uuid128[14], descr_uuid.uuid.uuid128[15]);
		} else {
			ble_logE("Descriptor UNKNOWN LEN %d", descr_uuid.len);
		} ble_logD("FOUND Descriptor pos %d handle %d", ble_add_char_pos,attr_handle);
		gl_char[ble_add_char_pos].descr_handle = attr_handle;
	}
	gatts_add_char();
}

static void gatts_check_callback(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	uint16_t handle = 0;
	uint8_t read = 1;

	switch (event) {
	case ESP_GATTS_READ_EVT: {
		read = 1;
		handle = param->read.handle;
		break;
	}
	case ESP_GATTS_WRITE_EVT: {
		read = 0;
		handle = param->write.handle;
		break;
	}
	default:
		break;
	}

	ble_logD("gatts_check_callback read %d num %d handle %d", read, GATTS_CHAR_NUM, handle);
	for (uint32_t pos = 0; pos < GATTS_CHAR_NUM; pos++) {
		if (gl_char[pos].char_handle == handle) {
			if (read == 1) {
				if (gl_char[pos].char_read_callback != NULL) {
					gl_char[pos].char_read_callback(event, gatts_if, param);
				}
			} else {
				if (gl_char[pos].char_write_callback != NULL) {
					gl_char[pos].char_write_callback(event, gatts_if, param);
				}
			}
			break;
		}
		if (gl_char[pos].descr_handle == handle) {
			if (read == 1) {
				if (gl_char[pos].descr_read_callback != NULL) {
					gl_char[pos].descr_read_callback(event, gatts_if, param);
				}
			} else {
				if (gl_char[pos].descr_write_callback != NULL) {
					gl_char[pos].descr_write_callback(event, gatts_if, param);
				}
			}
			break;
		}
	}
}

static void gap_event_handler(esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t *param) {
	switch (event) {
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
		esp_ble_gap_start_advertising(&ble_adv_params);
		break;
	default:
		break;
	}
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	switch (event) {
	case ESP_GATTS_REG_EVT:
		ble_logD("REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
		gl_profile.service_id.is_primary = true;
		gl_profile.service_id.id.inst_id = 0x00;
		gl_profile.service_id.id.uuid.len = ESP_UUID_LEN_128;
		for (uint8_t pos = 0; pos < ESP_UUID_LEN_128; pos++) {
			gl_profile.service_id.id.uuid.uuid.uuid128[pos] =
					ble_service_uuid128[pos];
		}
		// Changed - device name
		esp_ble_gap_set_device_name(mDeviceName);
		esp_err_t ret = esp_ble_gap_config_adv_data(&ble_adv_data);
		ble_logD("esp_ble_gap_config_adv_data %d", ret);

		esp_ble_gatts_create_service(gatts_if, &gl_profile.service_id,
				GATTS_NUM_HANDLE);
		break;
	case ESP_GATTS_READ_EVT: {
		ble_logD("GATT_READ_EVT, conn_id %d, trans_id %d, handle %d", param->read.conn_id, param->read.trans_id, param->read.handle);
//        esp_gatt_rsp_t rsp;
//        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
//        rsp.attr_value.handle = param->read.handle;
//        rsp.attr_value.len = 4;
//        rsp.attr_value.value[0] = 0x0A;
//        rsp.attr_value.value[1] = 0x0B;
//        rsp.attr_value.value[2] = 0x0C;
//        rsp.attr_value.value[3] = 0x0D;
//        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
//                                    ESP_GATT_OK, &rsp);
		gatts_check_callback(event, gatts_if, param);
		break;
	}
	case ESP_GATTS_WRITE_EVT: {
		ble_logD("GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d", param->write.conn_id, param->write.trans_id, param->write.handle);
		ble_logD("GATT_WRITE_EVT, value len %d, value %08x", param->write.len, *(uint32_t *)param->write.value);
//       esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
		gatts_check_callback(event, gatts_if, param);
		break;
	}
	case ESP_GATTS_MTU_EVT:

		// Original code changed here !

		ble_logD("ESP_GATTS_MTU_EVT, mtu %d", param->mtu.mtu);
		mMTU = param->mtu.mtu;

		// Callback for MTU changed

		if (mCallbackMTU != NULL) {

			mCallbackMTU();
		}

		break;
	case ESP_GATTS_EXEC_WRITE_EVT:
//	case ESP_GATTS_MTU_EVT:
	case ESP_GATTS_CONF_EVT:
	case ESP_GATTS_UNREG_EVT:
		break;
	case ESP_GATTS_CREATE_EVT:
		ble_logD("CREATE_SERVICE_EVT, status %d,  service_handle %d", param->create.status, param->create.service_handle);
		gl_profile.service_handle = param->create.service_handle;
		gl_profile.char_uuid.len = gl_char[0].char_uuid.len;
		gl_profile.char_uuid.uuid.uuid16 = gl_char[0].char_uuid.uuid.uuid16;

		esp_ble_gatts_start_service(gl_profile.service_handle);
		gatts_add_char();
//        esp_ble_gatts_add_char(gl_profile.service_handle, &gl_profile.char_uuid,
//                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
//                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
//                               &gatts_demo_char1_val, NULL);
		break;
	case ESP_GATTS_ADD_INCL_SRVC_EVT:
		break;
	case ESP_GATTS_ADD_CHAR_EVT: {
//	    uint16_t length = 0;
//        const uint8_t *prf_char;

		ble_logD("ADD_CHAR_EVT, status 0x%X,  attr_handle %d, service_handle %d",
				param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
		gl_profile.char_handle = param->add_char.attr_handle;
//        gl_profile.descr_uuid.len = ESP_UUID_LEN_16;
//        gl_profile.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
//        esp_ble_gatts_get_attr_value(param->add_char.attr_handle,  &length, &prf_char);

//        ble_logD("the gatts demo char length = %x", length);
//        for(int i = 0; i < length; i++){
//            ble_logD("prf_char[%x] =%x",i,prf_char[i]);
//        }
//        esp_ble_gatts_add_char_descr(gl_profile.service_handle, &gl_profile.descr_uuid,
//                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
		if (param->add_char.status == ESP_GATT_OK) {
			gatts_check_add_char(param->add_char.char_uuid,
					param->add_char.attr_handle);
		}
		break;
	}
	case ESP_GATTS_ADD_CHAR_DESCR_EVT:
		ble_logD("ADD_DESCR_EVT char, status %d, attr_handle %d, service_handle %d",
				param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
		ble_logD("ADD_DESCR_EVT desc, status %d, attr_handle %d, service_handle %d",
				param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
		if (param->add_char_descr.status == ESP_GATT_OK) {
			gatts_check_add_descr(param->add_char.char_uuid,
					param->add_char.attr_handle);
		}
		break;
	case ESP_GATTS_DELETE_EVT:
		break;
	case ESP_GATTS_START_EVT:
		ble_logD("SERVICE_START_EVT, status %d, service_handle %d",
				param->start.status, param->start.service_handle);
		break;
	case ESP_GATTS_STOP_EVT:
		break;
	case ESP_GATTS_CONNECT_EVT:
		ble_logD("SERVICE_START_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:, is_conn %d",
				param->connect.conn_id,
				param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
				param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5],
				param->connect.conn_id);
		gl_profile.conn_id = param->connect.conn_id;

		// Original code changed here !

		/// BLE

		mGatts_if = gatts_if;

		mConnected = true;

		// Callback for connection

		if (mCallbackConnection != NULL) {

			mCallbackConnection();
		}

		break;

	case ESP_GATTS_DISCONNECT_EVT:
		esp_ble_gap_start_advertising(&ble_adv_params);

		// Original code changed here !

		/// BLE

		mGatts_if = ESP_GATT_IF_NONE;
		mConnected = false;
		mMTU = 20;

		// Callback for connection

		if (mCallbackConnection != NULL) {

			mCallbackConnection();
		}

		break;
	case ESP_GATTS_OPEN_EVT:
	case ESP_GATTS_CANCEL_OPEN_EVT:
	case ESP_GATTS_CLOSE_EVT:
	case ESP_GATTS_LISTEN_EVT:
	case ESP_GATTS_CONGEST_EVT:
	default:
		break;
	}
}

static void gatts_event_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	/* If event is register event, store the gatts_if for each profile */
	if (event == ESP_GATTS_REG_EVT) {
		if (param->reg.status == ESP_GATT_OK) {
			gl_profile.gatts_if = gatts_if;
		} else {
			ble_logD("Reg app failed, app_id %04x, status %d",
					param->reg.app_id,
					param->reg.status);
			return;
		}
	}

	gatts_profile_event_handler(event, gatts_if, param);
}

///// End of pbcreflux codes

// Public

/**
* @brief Initialize ble and ble server
*/
esp_err_t ble_uart_server_Initialize (const char* device_name) {

	// Device Name

	strncpy (mDeviceName, device_name, 30 - 5); // leaves 5 reserved for the end of mac address

	ble_logD ("Initializing BLE - device name %s - cpu %d", mDeviceName, xPortGetCoreID ());

	// Initialize the BLE

	esp_err_t ret;

	// Free the memory of the classic BT (seen at https://www.esp32.com/viewtopic.php?f=13&t=3139);

	esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

	// Initialize the Bluetooth

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ret = esp_bt_controller_init (& bt_cfg);
	if (ret) {
		ble_logE("initialize controller failed");
        return ESP_FAIL;
	}

	ret = esp_bt_controller_enable (ESP_BT_MODE_BLE); // BLE -> BLE only
	if (ret) {
		ble_logE("enable controller failed");
        return ESP_FAIL;
	}

	// Initialize bluedroid

	ret = esp_bluedroid_init ();
	if (ret) {
		ble_logE("init bluetooth failed");
        return ESP_FAIL;
	}
	ret = esp_bluedroid_enable ();
	if (ret) {
		ble_logE("enable bluetooth failed");
        return ESP_FAIL;
	}

#ifdef BLE_TX_POWER
	// Changes transmission power - only for esp-idf v3.1 or higher

	ret = esp_ble_tx_power_set (ESP_BLE_PWR_TYPE_DEFAULT, BLE_TX_POWER);
	if (ret == ESP_OK) {
		ble_logD("esp_ble_tx_power_set: set to %d", BLE_TX_POWER);
	} else {
		ble_logE("esp_ble_tx_power_set: error ret=%d", ret);
	};
#endif

	// Get the mac adress

	mMacAddress = esp_bt_dev_get_address();

	// Change the name with the last two of the mac address
	// If name ends with '_'

	uint8_t size = strlen(mDeviceName);

	if (size > 0 && mDeviceName[size-1] == '_') {

		char aux[7];
		sprintf(aux, "%02X%02X", mMacAddress[4], mMacAddress[5]);

		strcat (mDeviceName, aux);
	}

	// Set the device name

	esp_bt_dev_set_device_name(mDeviceName);

	// Callbacks

	esp_ble_gatts_register_callback (gatts_event_handler);
	esp_ble_gap_register_callback (gap_event_handler);
	esp_ble_gatts_app_register (BLE_PROFILE_APP_ID);

	// Debug (logI to show allways)

	logI("BLE UART server initialized, device name %s", mDeviceName);

	return ESP_OK;
}

/**
* @brief Finalize ble server and ble 
*/
esp_err_t ble_uart_server_Finalize() {

	// TODO: verify if need something more

	esp_err_t ret = esp_bt_controller_deinit();

	// Debug (logI to show allways)

	logI("BLE UART server finalized");

	return ret;
}

/**
* @brief Set callback for conection/disconnection
*/
void ble_uart_server_SetCallbackConnection(void (*callbackConnection)(), void (*callbackMTU)()) {

	// Set the callbacks

	mCallbackConnection = callbackConnection;
	mCallbackMTU= callbackMTU;
}

/**
* @brief Set callback to receiving data
*/
void ble_uart_server_SetCallbackReceiveData(void (*callbackReceived) (char* data, uint16_t size)) {

	// Arrow callback to receive data

	mCallbackReceivedData = callbackReceived;
}

/**
* @brief Is a client connected to UART server?
*/
bool ble_uart_server_ClientConnected () {

	// Client connected to UART server?

	return mConnected;
}

/**
* @brief Send data to client (mobile App)
*/
esp_err_t ble_uart_server_SendData(const char* data, uint16_t size) {

	ble_logD ("data [%d] : %s", size, data);

	// Connected?

	if (mConnected == false || mGatts_if == ESP_GATT_IF_NONE) {
		ble_logE ("not connected");
		return ESP_FAIL;
	}

	// Check the size

	if (size > GATTS_CHAR_VAL_LEN_MAX) {
		size = GATTS_CHAR_VAL_LEN_MAX;
	}

	// Copy the data

	char send [GATTS_CHAR_VAL_LEN_MAX + 1];
	memset (send, 0, GATTS_CHAR_VAL_LEN_MAX + 1);
	strncpy (send, data, (size_t) size);

	// Send data via BLE notification

	ble_logD("if %d conn %d handle %d", mGatts_if, 0 , gl_char[1].char_handle);

	return esp_ble_gatts_send_indicate(mGatts_if, 0, gl_char[1].char_handle,
			size, (uint8_t *) send, false);

}

/**
* @brief Get actual MTU of client (mobile app)
*/
uint16_t ble_uart_server_MTU() {

	// Returns the received MTU or default

	return mMTU;

}

/**
* @brief Get mac address of ESP32
*/
const uint8_t* ble_uart_server_MacAddress() {

	return mMacAddress;

}

//////// End
