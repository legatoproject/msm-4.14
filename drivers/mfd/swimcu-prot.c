/************
 *
 * Filename:  swimcu-prot.c
 *
 * Purpose:   Implementation of microcontroller interface communication protocol
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
 *            All rights reserved
 *
 ************/

#include <linux/i2c.h>
#include <linux/delay.h>

#include <linux/mfd/swimcu/core.h>
#include <linux/mfd/swimcu/mciprotocol.h>
#include <linux/mfd/swimcu/mcidefs.h>

/*****************
 * Local defines *
 *****************/

/* Control of synchronous wait time for an expected byte.
 * Max wait time >= MCI_PROTOCOL_RECV_WAIT_INTERVAL * MCI_PROTOCOL_RECV_WAIT_CYCLES
 */
#define MCI_PROTOCOL_RECV_WAIT_INTERVAL   100  /* microseconds          */
#define MCI_PROTOCOL_RECV_WAIT_CYCLES     100  /* number of read cycles */
#define MCI_PROTOCOL_SEND_RETRIES         5
#define MCI_PROTOCOL_SEND_WAIT_LOW        750 /* min sleep, usec */
#define MCI_PROTOCOL_SEND_WAIT_HIGH       1500 /* max sleep, usec */

/* The index of decoded fields into the parameter list for PING_RESP */
#define MCI_PROTOCOL_PING_RESP_PARAMS_VER_MINOR     0
#define MCI_PROTOCOL_PING_RESP_PARAMS_VER_MAJOR     1
#define MCI_PROTOCOL_PING_RESP_PARAMS_TARGET        2
#define MCI_PROTOCOL_PING_RESP_PARAMS_OPT           3
#define MCI_PROTOCOL_PING_RESP_PARAMS_COUNT         4

/**************
 * Local data *
 **************/

/*************
 * Functions *
 *************/
static int mci_protocol_send(const struct i2c_client *client, const char *buf, int count)
{
	int ret;
	int sent = 0;
	int retries = MCI_PROTOCOL_SEND_RETRIES;

	do {
		ret = i2c_master_send(client, buf+sent, count-sent);
		if (ret > 0) {
			sent += ret;
		}
		if ((sent >= count) || (retries-- <= 0))
			break;
		usleep_range(MCI_PROTOCOL_SEND_WAIT_LOW, MCI_PROTOCOL_SEND_WAIT_HIGH);
	} while (true);

	if (retries < MCI_PROTOCOL_SEND_RETRIES)
		swimcu_log(PROT, "%s: i2c retries %d\n", __func__, MCI_PROTOCOL_SEND_RETRIES - retries);
	if (sent > 0)
		ret = sent;

	return ret;
}

/************
*
* Name:     mci_protocol_read_byte_wait
*
* Purpose:  To wait on i2c for an expected byte within given time
*
* Parms:    swimcu   -
*           byte     - expected byte to be received
*           interval - interval between subsequent reads (in micro-second)
*           count    - max number of reads
*
* Return:   TRUE if successful; FALSE otherwise.
*
* Abort:    none
*
* Notes:
*
************/
static bool mci_protocol_read_byte_wait(
	struct swimcu *swimcu,
	uint8_t  byte,
	uint32_t interval,
	uint32_t count)
{
	bool status = false;
	uint8_t   recv_byte = 0;
	int ret;

	/* Wait for arrival of the byte */
	while (count-- > 0) {
		ret = i2c_master_recv(swimcu->client, &recv_byte, 1);
		if (recv_byte == byte) {
			status = true;
			break;
		}
		usleep_range(interval, interval*2);
	}

	if (count < MCI_PROTOCOL_RECV_WAIT_CYCLES-1)
		swimcu_log(PROT, "%s: cnt = %d", __func__, count);

	return status;
}

/************
*
* Name:     mci_protocol_crc16_update
*
* Purpose:  To update CRC-16 for data in a given buffer
*
* Parms:    crc   - initial CRC value
*           datap - pointer to the beginning of the data buffer
*           len   - number of bytes in the data buffer
*
* Return:   updated CRC-16 value
*
* Abort:    none
*
* Notes:    not compatible with the linux kernel crc16
*
************/
static uint16_t mci_protocol_crc16_update(
	uint16_t crc,
	const uint8_t * datap,
	uint32_t len)
{
	uint16_t i, j, byte, temp;

	for (i = 0; i < len; ++i) {
		byte = datap[i];
		crc ^= byte << 8;
		for (j = 0; j < 8; ++j) {
			temp = crc << 1;
			if (crc & 0x8000) {
				temp ^= 0x1021;
			}
			crc = temp;
		}
	}
	return crc;
}

/************
*
* Name:     mci_protocol_frame_encode
*
* Purpose:  To encode a protocol frame to a flat buffer
*
* Parms:    framep  - pointer to protocol frame data
*           buffer  - pointer to the beginning of the flat buffer
*           buf_len - length of the flat buffer
*
* Return:   encoded data length if successful; 0 otherwise.
*
* Abort:    none
*
* Notes:    Callers to make sure the buffers are allocated properly
*
************/
static uint8_t mci_protocol_frame_encode (
	struct mci_protocol_frame_s *framep,
	uint8_t                     *bufferp,
	uint8_t                      buf_len)
{
	struct mci_protocol_packet_s *packetp = NULL;
	uint8_t  *encodep = bufferp;
	uint32_t *paramsp = NULL;
	uint32_t  temp;
	uint8_t   i;

	if (!framep || !bufferp || buf_len < MCI_PROTOCOL_FRAME_LEN_MAX) {
		return 0;
	}

	*encodep++ = MCI_PROTOCOL_FRAME_START_BYTE;
	*encodep++ = framep->type;

	switch (framep->type) {
		case MCI_PROTOCOL_FRAME_TYPE_APPL_ACK:
		case MCI_PROTOCOL_FRAME_TYPE_APPL_NAK:
		case MCI_PROTOCOL_FRAME_TYPE_APPL_ACK_ABORT:
		case MCI_PROTOCOL_FRAME_TYPE_PING_REQ:

		/* end of encoding */
			break;

		case MCI_PROTOCOL_FRAME_TYPE_APPL_COMMAND:
		case MCI_PROTOCOL_FRAME_TYPE_APPL_DATA:

			if(NULL == framep->payloadp ||
				framep->payload_len > MCI_PROTOCOL_PACKET_LEN_MAX) {
				encodep = bufferp;
				swimcu_log(PROT, "No payload for COMMAND/DATA frames");
				break;
			}

			*encodep++ = framep->payload_len & 0xFF;
			*encodep++ = (framep->payload_len >> 8) & 0xFF;

			/* Run CRC over frame header exclude CRC itself */
			framep->crc = mci_protocol_crc16_update(0, bufferp, MCI_PROTOCOL_FRAME_HEADER_LEN - 2);

			/* Skip frame CRC fields in the buffer */
			encodep += 2;

			if (framep->type == MCI_PROTOCOL_FRAME_TYPE_APPL_COMMAND) {
				packetp = (struct mci_protocol_packet_s*) framep->payloadp;

				/* encode packet header */
				*encodep++ = packetp->tag;
				*encodep++ = packetp->flags;
				*encodep++ = 0;
				*encodep++ = packetp->count;

				/* encode parameters if any */
				if (packetp->count > 0) {
					paramsp = (uint32_t *) packetp->datap;
					for (i = 0; i < packetp->count; i++) {
						temp = paramsp[i];
						*encodep++ = temp & 0xFF;
						temp >>= 8;
						*encodep++ = temp & 0xFF;
						temp >>= 8;
						*encodep++ = temp & 0xFF;
						temp >>= 8;
						*encodep++ = temp & 0xFF;
					}
					swimcu_log(PROT, "%s: encode params %d: %08X %08X\n", __func__, packetp->count, paramsp[0], paramsp[1]);
				}
			}

			/* update CRC with payload data */
			framep->crc = mci_protocol_crc16_update(framep->crc,
				(bufferp + MCI_PROTOCOL_FRAME_HEADER_LEN),
				framep->payload_len);

			/* encode the CRC in the frame header */
			bufferp[MCI_PROTOCOL_FRAME_HEADER_CRC16_LO] = framep->crc & 0xFF;
			bufferp[MCI_PROTOCOL_FRAME_HEADER_CRC16_HI] = (framep->crc >> 8) & 0xFF;

			break;

		case MCI_PROTOCOL_FRAME_TYPE_PING_RESP:
		case MCI_PROTOCOL_FRAME_TYPE_INVALID:
		default:

			encodep = bufferp;
			pr_err("Unhandled frame type 0x%x ", framep->type);
			break;
	}

	swimcu_log(PROT, "%s: frame 0x%x, length=%d\n", __func__, framep->type, (encodep - bufferp));
	if (packetp && (packetp->count > 0))
		swimcu_log(PROT, "encode params %d: %08X %08X %08X\n", packetp->count, paramsp[0], paramsp[1], paramsp[2]);

	return (encodep - bufferp);
}

/************
*
* Name:     mci_protocol_frame_send
*
* Purpose:  To send a protocol frame to microcontroller
*
* Parms:    *swimcu
*           framep - pointer to the protocol frame to be sent
*
* Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
*           error code of mci_protocol_status_code_e type otherwise.
*
* Abort:    none
*
* Notes:    FD must have been opened and configured.
*
************/
static enum mci_protocol_status_code_e mci_protocol_frame_send(
	struct swimcu *swimcu,
	struct mci_protocol_frame_s *framep)
{
	uint8_t  buffer[MCI_PROTOCOL_FRAME_LEN_MAX];
	uint8_t  encoded_len;
	int ret;

	if (!framep) {
		pr_err("NULL frame object");
		return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
	}

	encoded_len = mci_protocol_frame_encode(framep, buffer, MCI_PROTOCOL_FRAME_LEN_MAX);
	if (encoded_len <= 0) {
		pr_err("Failed to encode the frame\n");
		return MCI_PROTOCOL_STATUS_CODE_ENCODE_ERROR;
	}

	/* write encoded frame to the microcontroller */
	ret = mci_protocol_send(swimcu->client, buffer, encoded_len);
	if (ret != encoded_len) {
		pr_err("%s: write frame fail to I2C: %d of %d", __func__, ret, encoded_len);
		swimcu_set_fault_mask(SWIMCU_FAULT_TX_TO);
		return MCI_PROTOCOL_STATUS_CODE_TX_ERROR;
	}

	swimcu_log(PROT, "%s: success %d bytes: %02X %02X %02X...\n", __func__, ret, buffer[0], buffer[1], buffer[2]);
	return MCI_PROTOCOL_STATUS_CODE_SUCCESS;
}

/************
*
* Name:     mci_protocol_frame_recv
*
* Purpose:  To receive a prtocol frame from the microcontroller
*
* Parms:    swimcu
*           framep - pointer to protocol frame C structure to receive
*
* Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
*           error code of mci_protocol_status_code_e type otherwise.
*
* Abort:    none
*
* Notes:
*
************/
static enum mci_protocol_status_code_e mci_protocol_frame_recv (
	struct swimcu *swimcu,
	struct mci_protocol_frame_s  *framep)
{
	enum mci_protocol_status_code_e s_code = MCI_PROTOCOL_STATUS_CODE_FAIL;
	struct mci_protocol_packet_s *packetp = NULL;

	uint8_t   buffer[MCI_PROTOCOL_FRAME_LEN_MAX];
	uint8_t  *datap = NULL;
	uint8_t  *decodep = NULL;
	uint32_t *params = NULL;
	uint16_t  crc;
	int len = 0;
	int i;

	if (!framep) {
		pr_err("NULL frame object (%d)",
			MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT);
		return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
	}

	/* Wait on I2C for the start byte */
	if (!mci_protocol_read_byte_wait(swimcu, MCI_PROTOCOL_FRAME_START_BYTE,
				MCI_PROTOCOL_RECV_WAIT_INTERVAL,
				MCI_PROTOCOL_RECV_WAIT_CYCLES)) {
		swimcu_set_fault_mask(SWIMCU_FAULT_RX_TO);
		pr_err("Failed to receive frame from MCU %d\n",
			MCI_PROTOCOL_STATUS_CODE_RX_ERROR);
		return MCI_PROTOCOL_STATUS_CODE_RX_ERROR;
	}
	buffer[MCI_PROTOCOL_FRAME_HEADER_START] = MCI_PROTOCOL_FRAME_START_BYTE;

	/* Read the frame type byte */
	buffer[MCI_PROTOCOL_FRAME_HEADER_TYPE] = MCI_PROTOCOL_FRAME_TYPE_INVALID;
	len = i2c_master_recv(swimcu->client, &(buffer[MCI_PROTOCOL_FRAME_HEADER_TYPE]), 1);
	framep->type = (enum mci_protocol_frame_type_e) buffer[MCI_PROTOCOL_FRAME_HEADER_TYPE];
	swimcu_log(PROT, "%s: header type: %02X\n", __func__, buffer[MCI_PROTOCOL_FRAME_HEADER_TYPE]);
	switch (framep->type) {
		case MCI_PROTOCOL_FRAME_TYPE_APPL_NAK:
			swimcu_set_fault_mask(SWIMCU_FAULT_TX_NAK);
		case MCI_PROTOCOL_FRAME_TYPE_APPL_ACK:
		case MCI_PROTOCOL_FRAME_TYPE_APPL_ACK_ABORT:

			/* Short frame: end of receiving */
			s_code = MCI_PROTOCOL_STATUS_CODE_SUCCESS;
		break;

		case MCI_PROTOCOL_FRAME_TYPE_PING_RESP:

			/* Read the rest of bytes in the frame */
			len = i2c_master_recv(swimcu->client,
				&(buffer[MCI_PROTOCOL_FRAME_PING_RESP_PROT_VER]),
				MCI_PROTOCOL_FRAME_PING_RESP_LEN - 2);

			/* Run CRC16 on the buffer excluding CRC bytes (last two bytes) */
			crc = mci_protocol_crc16_update(0, buffer, (MCI_PROTOCOL_FRAME_PING_RESP_LEN - 2));

			/* Decode CRC carried in the resp */
			framep->crc = 0x00FF & buffer[MCI_PROTOCOL_FRAME_PING_RESP_CRC16_HI];
			framep->crc <<= 8;
			framep->crc |= 0x00FF & buffer[MCI_PROTOCOL_FRAME_PING_RESP_CRC16_LO];

			if (framep->crc != crc) {
				swimcu_set_fault_mask(SWIMCU_FAULT_RX_CRC);
				pr_err("Response CRC mismatch 0x%x (0x%x)", crc, framep->crc);
				break;
			}

			/* Decode the payload of the frame */
			if (!framep->payloadp) {
				pr_err("No packet structure for receiving RESP");
				break;
			}

			packetp = (struct mci_protocol_packet_s*) framep->payloadp;
			if (!packetp->datap) {
				pr_err("No buffer for receiving parameters list");
				break;
			}
			params = (uint32_t *) packetp->datap;

			params[MCI_PROTOCOL_PING_RESP_PARAMS_VER_MINOR] = buffer[MCI_PROTOCOL_FRAME_PING_RESP_FWVER_MINOR];
			params[MCI_PROTOCOL_PING_RESP_PARAMS_VER_MAJOR] = buffer[MCI_PROTOCOL_FRAME_PING_RESP_FWVER_MAJOR];
			swimcu_log(PROT, "%s: prot_ver= %d\n", __func__, buffer[MCI_PROTOCOL_FRAME_PING_RESP_PROT_VER]);
			if (buffer[MCI_PROTOCOL_FRAME_PING_RESP_PROT_VER] == MCI_PROTOCOL_VERSION_1)
			{
				params[MCI_PROTOCOL_PING_RESP_PARAMS_TARGET]= buffer[MCI_PROTOCOL_FRAME_PING_RESP_TARGET];
				params[MCI_PROTOCOL_PING_RESP_PARAMS_OPT]   = buffer[MCI_PROTOCOL_FRAME_PING_RESP_OPT_HI];
				params[MCI_PROTOCOL_PING_RESP_PARAMS_OPT]  <<= 8;
				params[MCI_PROTOCOL_PING_RESP_PARAMS_OPT]   |= (uint32_t) buffer[MCI_PROTOCOL_FRAME_PING_RESP_OPT_LO];
			}
			else
			{
				/* Use default values */
				params[MCI_PROTOCOL_PING_RESP_PARAMS_TARGET]= MCI_PROTOCOL_APPL_TARGET;
				params[MCI_PROTOCOL_PING_RESP_PARAMS_OPT]   = 0x0;
				if (buffer[MCI_PROTOCOL_FRAME_PING_RESP_PROT_VER] == MCI_PROTOCOL_VERSION_0)
				{
					pr_err("Unkown MCI Protocol Version");
				}
			}

			packetp->count = MCI_PROTOCOL_PING_RESP_PARAMS_COUNT;

			s_code = MCI_PROTOCOL_STATUS_CODE_SUCCESS;
			break;

		case MCI_PROTOCOL_FRAME_TYPE_APPL_COMMAND:
		case MCI_PROTOCOL_FRAME_TYPE_APPL_DATA:

		/* read the rest of the frame header */
			len = i2c_master_recv(swimcu->client, &(buffer[2]), MCI_PROTOCOL_FRAME_HEADER_LEN - 2);

			/* decode the frame header */
			framep->payload_len = buffer[MCI_PROTOCOL_FRAME_HEADER_PAYLOAD_LEN_HI];
			framep->payload_len <<= 8;
			framep->payload_len |= (uint32_t) buffer[MCI_PROTOCOL_FRAME_HEADER_PAYLOAD_LEN_LO];

			framep->crc = buffer[MCI_PROTOCOL_FRAME_HEADER_CRC16_HI];
			framep->crc <<= 8;
			framep->crc |= (uint32_t) buffer[MCI_PROTOCOL_FRAME_HEADER_CRC16_LO];

			swimcu_log(PROT, "%s: header read %d, pl %d, crc %04X\n", __func__, len, framep->payload_len, framep->crc);

			/* Run CRC16 on the frame header, excluding last 2 bytes of CRC */
			crc = mci_protocol_crc16_update(0, buffer, (MCI_PROTOCOL_FRAME_HEADER_LEN- 2));
			if (framep->payload_len > MCI_PROTOCOL_FRAME_LEN_MAX) {
				pr_err("Frame payload len 0x%x overruns buffer size (0x%x)",
					framep->payload_len, MCI_PROTOCOL_FRAME_LEN_MAX);
				break;
			}

			/* Read payload for the command/data frame */
			len = i2c_master_recv(swimcu->client, buffer, framep->payload_len);
			swimcu_log(PROT, "%s: len %d, %02X %02X %02X...\n", __func__, len, buffer[0], buffer[1], buffer[2]);

			if (len != framep->payload_len) {
				pr_err("Failed to read from I2C %d (%d)\n", len, framep->payload_len);
				break;
			}

			/* Update CRC16 with the payload data */
			crc = mci_protocol_crc16_update(crc, buffer, len);
			if (crc != framep->crc) {
				pr_err("CRC mismatch 0x%x (0x%x)", crc, framep->crc);
				break;
			}

			if (framep->type == MCI_PROTOCOL_FRAME_TYPE_APPL_DATA) {
			/* DATA frame has no pakcet header. Simply copy out
			 * the payload data from the flat buffer
			 */
				if (!framep->payloadp) {
					pr_err("No buffer for returned data");
					break;
				}

				datap = (uint8_t*) framep->payloadp;
				decodep = buffer;
				i = framep->payload_len;
				while(i--) {
					*datap++ = *decodep++;
				}

			/* TO-BE-TESTED: no DATA frame type from micro-controller yet */
				s_code = MCI_PROTOCOL_STATUS_CODE_SUCCESS;
				break;
			}

			/* Decode received data for COMMAND response frame */
			if (!framep->payloadp) {
				pr_err("No packet structure for frame payload");
				break;
			}
			packetp = (struct mci_protocol_packet_s*) framep->payloadp;

			/* Decode packet header */
			decodep = buffer;
			packetp->tag = *decodep++;
			packetp->flags = *decodep++;
			decodep++;                  /* skip MCI_PROTOCOL_PACKET_HEADER_FIELD_RSVD */
			packetp->count = *decodep++;

			/* sanity check */
			if (packetp->tag != MCI_PROTOCOL_COMMAND_TAG_GENERIC_RESP) {
				pr_err("Invalid response tag 0x%x", packetp->tag);
				break;
			}

			if (packetp->count > 0) {
			/* Check user-provided storage for returned value */
				if(!packetp->datap) {
					pr_err("No buffer for parameter list");
					break;
				}
				params = (uint32_t*) packetp->datap;

				/* Decode the parameters into provided storage */
				for (i = 0; i < packetp->count; i++) {
					params[i] = decodep[3];
					params[i] <<= 8;
					params[i] |= (uint32_t) decodep[2];
					params[i] <<= 8;
					params[i] |= (uint32_t) decodep[1];
					params[i] <<= 8;
					params[i] |= (uint32_t) decodep[0];
					decodep += 4;
				}
				swimcu_log(PROT, "%s: decode params %d:\n", __func__, packetp->count);
				swimcu_log(PROT, " %08X %08X %08X %08X\n", params[0], params[1], params[2], params[3]);
			}

			s_code = MCI_PROTOCOL_STATUS_CODE_SUCCESS;
			break;

		default:

			pr_err("Unhandled frame type %d", framep->type);
			break;
	}

	swimcu_log(PROT, "%s: success\n", __func__);
	return s_code;
}

/************
 *
 * Name:     mci_protocol_command
 *
 * Purpose:  Command interface to microcontroller
 *
 * Parms:    swimcu     [IN]
 *           cmd        [IN]  - tag of the command to be issued
 *           params     [IN]  - parameters for the command
 *                      [OUT] - results returned from bootloader
 *           params_max [IN]  - max number of parameters
 *           countp     [IN]  - number of parameters (uint32_t) to send
 *                      [OUT] - number of results (uint32_t) returned
 *           flags      [IN]  - flag to indicate if data phase will follow
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           error status code otherwise.
 *
 * Abort:    none
 *
 ************/
enum mci_protocol_status_code_e mci_protocol_command(
	struct swimcu *swimcu,
	enum mci_protocol_command_tag_e cmd,
	uint32_t  params[],
	uint8_t   params_max,
	uint8_t  *countp,
	uint8_t   flags)
{
	enum mci_protocol_status_code_e s_code;
	struct mci_protocol_frame_s frame;
	struct mci_protocol_packet_s packet;
	uint32_t results[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	int    i;

	if (swimcu == NULL) {
		return MCI_PROTOCOL_STATUS_CODE_FAIL;
	}

	mutex_lock(&swimcu->mcu_transaction_mutex);

	packet.tag   = cmd;
	packet.datap = params;
	if (NULL == params || NULL == countp) {
		packet.count = 0;
	}
	else {
		packet.count = *countp;
	}
	packet.flags = flags;

	frame.payloadp = (void*) &packet;
	frame.payload_len = MCI_PROTOCOL_PACKET_HEADER_LEN + packet.count * sizeof(uint32_t);
	frame.type = MCI_PROTOCOL_FRAME_TYPE_APPL_COMMAND;

	s_code = mci_protocol_frame_send(swimcu, &frame);
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != s_code) {
		pr_err("Failed to send request %.2x (%d)\n", cmd, s_code);
		goto exit;
	}

	/* Receive next frame ACK/NAK */
	frame.type = MCI_PROTOCOL_FRAME_TYPE_INVALID;
	s_code = mci_protocol_frame_recv(swimcu, &frame);
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != s_code) {
		pr_err("Failed to receive a frame %d\n", s_code);
		goto exit;
	}

	/* Check the received frame type */
	if (frame.type != MCI_PROTOCOL_FRAME_TYPE_APPL_ACK) {
		pr_err("Received unexpected frame type %.2x (%.2x)",
			frame.type, MCI_PROTOCOL_FRAME_TYPE_APPL_ACK);
		s_code = MCI_PROTOCOL_STATUS_CODE_FAIL;
		goto exit;
	}

	/* Initialize the frame buffer before receiving */
	for (i = 0; i < MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX; i++) {
		results[i] = 0;
	}
	packet.datap = results;
	packet.count = MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX;
	frame.type = MCI_PROTOCOL_FRAME_TYPE_INVALID;

	/* Receive response to the COMMAND */
	s_code = mci_protocol_frame_recv(swimcu, &frame);
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != s_code) {
		/* NAK the response */
		frame.type = MCI_PROTOCOL_FRAME_TYPE_APPL_NAK;
		(void)mci_protocol_frame_send(swimcu, &frame);

		pr_err("Failed to recv response\n");
		s_code = MCI_PROTOCOL_STATUS_CODE_RX_ERROR;
		goto exit;
	}

	/* ACK the response */
	frame.type = MCI_PROTOCOL_FRAME_TYPE_APPL_ACK;

	s_code = mci_protocol_frame_send(swimcu, &frame);
	if (s_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
		pr_err("Failed to send ACK %d\n", s_code);
		goto exit;
	}

	if (packet.tag != MCI_PROTOCOL_COMMAND_TAG_GENERIC_RESP) {
		pr_err("Unexpected response tag = 0x%x (0x%x)",
		packet.tag, MCI_PROTOCOL_COMMAND_TAG_GENERIC_RESP);
		s_code = MCI_PROTOCOL_STATUS_CODE_FAIL;
		goto exit;
	}

	/* GENERIC RESP has always two parameters: status and corresponding cmd
	 * Expand the protocol to allow other parameters from application command
	 */
	if (packet.count < MCI_PROTOCOL_GENERIC_RESP_RESULT_COUNT_MIN) {
		pr_err("Unexpected number of parameters = %d (%d)",
		packet.count, MCI_PROTOCOL_GENERIC_RESP_RESULT_COUNT_MIN);
		s_code = MCI_PROTOCOL_STATUS_CODE_FAIL;
		goto exit;
	}

	/* Verify the second parameter matches the command */
	if (cmd != ((enum mci_protocol_command_tag_e) results[1])) {
		pr_err("Incorrect response type to cmd %.2x (%.2x)", results[1], cmd);
		s_code = MCI_PROTOCOL_STATUS_CODE_FAIL;
		goto exit;
	}

	/* extract the status from the first parameter */
	s_code = (enum mci_protocol_status_code_e) results[0];

	/* extract returned results if there is any */
	if (packet.count > MCI_PROTOCOL_GENERIC_RESP_RESULT_COUNT_MIN) {
		/* check user provided buffer */
		if (!params || !countp) {
			swimcu_log(PROT, "command %.2x return count %d, discard", packet.tag, packet.count);
			goto exit;
		}

		/* copy the results */
		packet.count -= MCI_PROTOCOL_GENERIC_RESP_RESULT_COUNT_MIN;
		for (i = 0; i < packet.count; i++) {
			params[i] = results[i + MCI_PROTOCOL_GENERIC_RESP_RESULT_COUNT_MIN];
		}
		*countp = packet.count;
	}
	else {
		*countp = 0;
	}

exit:
	mutex_unlock(&swimcu->mcu_transaction_mutex);

	return s_code;
}

/************
 *
 * Name:     swimcu_ping
 *
 * Purpose:  Generate a ping message to MCU and retrieve MCU version.
 *
 * Parms:    swimcu - driver data block
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           error status code otherwise.
 *
 * Abort:    on MCU communication error
 *
 * Notes:    called on startup and on sysfs version query
 *
 ************/
enum mci_protocol_status_code_e swimcu_ping(struct swimcu *swimcu)
{
	enum mci_protocol_status_code_e s_code;
	struct mci_protocol_frame_s  frame;
	struct mci_protocol_packet_s packet;
	uint32_t params[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];

	mutex_lock(&swimcu->mcu_transaction_mutex);

	swimcu->version_major = 0;
	swimcu->version_minor = 0;
	swimcu->target_dev_id = 0;
	swimcu->opt_func_mask = 0x0;

	/* Ping the micro-controller and wait for response */
	frame.type = MCI_PROTOCOL_FRAME_TYPE_PING_REQ;
	s_code = mci_protocol_frame_send(swimcu, &frame);
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != s_code) {
		pr_err("%s: Failed to send PING\n", __func__);
		goto ping_exit;
	}

	/* Initialize the receiving buffer and count with max number of params */
	packet.count = 0;
	while (packet.count < MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX)
	{
		params[packet.count] = 0;
		packet.count++;
	}
	packet.datap = params;
	frame.payloadp = &packet;
	frame.type = MCI_PROTOCOL_FRAME_TYPE_INVALID;

	/* Receive Ping response from micro-controller */
	s_code = mci_protocol_frame_recv(swimcu, &frame);
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != s_code) {
		pr_err("%s: Failed to receive PING RESPONSE err=%d", __func__, s_code);
	}
	else if (MCI_PROTOCOL_FRAME_TYPE_PING_RESP != frame.type) {
		s_code = MCI_PROTOCOL_STATUS_CODE_CODING_ERROR;
		pr_err("%s: Unexpected frame type %.2x", __func__, frame.type);
	}

	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS == s_code) {
		swimcu->version_major =
			(u8) params[MCI_PROTOCOL_PING_RESP_PARAMS_VER_MAJOR];
		swimcu->version_minor =
			(u8) params[MCI_PROTOCOL_PING_RESP_PARAMS_VER_MINOR];
		swimcu->target_dev_id =
			(u8) params[MCI_PROTOCOL_PING_RESP_PARAMS_TARGET];
		swimcu->opt_func_mask=
			(u16) params[MCI_PROTOCOL_PING_RESP_PARAMS_OPT];
	}
	else {
		/* Failed: reset state relevant variables */
		swimcu->version_major = 0;
		swimcu->version_minor = 0;
		swimcu->target_dev_id = 0;
		swimcu->opt_func_mask = 0x0;
	}

	swimcu_log(FW, "%s: ver %d.%03d target %d opt_func_mask 0x%X\n", __func__,
		swimcu->version_major, swimcu->version_minor,
		swimcu->target_dev_id, swimcu->opt_func_mask);

ping_exit:
	mutex_unlock(&swimcu->mcu_transaction_mutex);
	return s_code;
}

/************
 *
 * Name:     swimcu_to_boot_transit
 *
 * Purpose:  To transit MCU operation mode from APPL to BOOT
 *
 * Parms:    none
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    MCU must be in APPL operation mode.
 *
 ************/
enum mci_protocol_status_code_e swimcu_to_boot_transit(
	struct swimcu *swimcu)
{
	enum mci_protocol_status_code_e s_code;
	uint8_t count;

	count = 0;
	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_START_BOOTLOADER,
		NULL, 0, &count, 0x00);

	swimcu_log(FW, "%s: %d\n", __func__, s_code);

	return s_code;
}

/************
 *
 * Name:     mci_appl_pin_config_set
 *
 * Purpose:  To set configuration for an MCU pin
 *
 * Parms:    pin_configp - pointers to configuration data of the MCU pin
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_pin_config_set(
	struct swimcu *swimcu,
	uint8_t port_number,
	uint8_t pin_number,
	struct mci_mcu_pin_state_s * pin_configp)
{
	enum mci_protocol_status_code_e s_code;
	uint32_t buffer[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	uint32_t attr;
	uint8_t  count;

	/* two 32-bit parameters required for the command */
	count = MCI_PROTOCOL_PIN_CONFIG_SET_ARG_COUNT;

	/* encode pin identification and operation type in the first parameter */
	buffer[0] = (((uint32_t)port_number) <<  MCI_PROTOCOL_PIN_SERVICE_PORT_NUM_SHIFT) |
		    (((uint32_t)pin_number)  <<  MCI_PROTOCOL_PIN_SERVICE_PIN_NUM_SHIFT)  |
		    (MCI_PROTOCOL_PIN_SERVICE_OPTYPE_CONFIG_SET << MCI_PROTOCOL_PIN_SERVICE_OPTYPE_SHIFT);

	/* encode GPIO direction and level in lowest byte
	 * (needed only for output pin, already defaulted to input low above)
	 */
	if (pin_configp->dir == MCI_MCU_PIN_DIRECTION_OUTPUT) {
		buffer[0] |= MCI_PROTOCOL_PIN_GPIO_DIRECTION_MASK;
		if (pin_configp->level == MCI_MCU_PIN_LEVEL_HIGH) {
			buffer[0] |= MCI_PROTOCOL_PIN_GPIO_LEVEL_MASK;
		}
	}

	/* encode port control register value for the pin in the second parameter */
	buffer[1]  = ((uint32_t)pin_configp->mux) << MCI_PROTOCOL_PIN_CONFIG_FUNCTION_MUX_SHIFT;
	buffer[1] &= MCI_PROTOCOL_PIN_CONFIG_FUNCTION_MUX_MASK;

	if (pin_configp->dir == MCI_MCU_PIN_DIRECTION_OUTPUT) {
		attr = (uint32_t) pin_configp->params.output.sre;
		buffer[1] |= (attr << MCI_PROTOCOL_PIN_CONFIG_SLEW_RATE_SHIFT) &
			MCI_PROTOCOL_PIN_CONFIG_SLEW_RATE_MASK;

		attr = (uint32_t) pin_configp->params.output.dse;
		buffer[1] |= (attr << MCI_PROTOCOL_PIN_CONFIG_DRIVE_STRENGTH_SHIFT) &
			MCI_PROTOCOL_PIN_CONFIG_DRIVE_STRENGTH_MASK;
	}
	else {
		attr = (uint32_t) pin_configp->params.input.pe;
		buffer[1] |= (attr << MCI_PROTOCOL_PIN_CONFIG_PULL_ENABLE_SHIFT) &
			MCI_PROTOCOL_PIN_CONFIG_PULL_ENABLE_MASK;

		attr = (uint32_t) pin_configp->params.input.ps;
		buffer[1] |= (attr << MCI_PROTOCOL_PIN_CONFIG_PULL_SELECT_SHIFT) &
			MCI_PROTOCOL_PIN_CONFIG_PULL_SELECT_MASK;

		attr = (uint32_t) pin_configp->params.input.pfe;
		buffer[1] |= (attr << MCI_PROTOCOL_PIN_CONFIG_PASSIVE_FILTER_SHIFT) &
			MCI_PROTOCOL_PIN_CONFIG_PASSIVE_FILTER_MASK;

		if (pin_configp->params.input.irqc_type != MCI_PIN_IRQ_DISABLED) {
			attr = (uint32_t) pin_configp->params.input.irqc_type;
			buffer[1] |= (attr << MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_SHIFT) &
				MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_MASK;
		}
	}

	/* Expect status code only; no returned results */
	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_PIN_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX,
		&count, 0x00);

	swimcu_log(PROT, "%s: status %d, dir %d, level %d\n", __func__, s_code, pin_configp->dir, pin_configp->level);

	return s_code;
}


/************
 *
 * Name:     swimcu_pin_states_get
 *
 * Purpose:  To get states of an MCU pin
 *
 * Parms:    pin_statep - pointers to storage for returned pin states
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_pin_states_get(
	struct swimcu *swimcu,
	uint8_t port_number,
	uint8_t pin_number,
	struct mci_mcu_pin_state_s * pin_statep)

{
	enum mci_protocol_status_code_e s_code;
	uint32_t buffer[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	uint32_t field;
	uint8_t  count;

	/* encode pin identification and operation type in the parameter */
	count = MCI_PROTOCOL_PIN_STATES_GET_ARG_COUNT;
	buffer[0] =
		(((uint32_t)port_number) <<  MCI_PROTOCOL_PIN_SERVICE_PORT_NUM_SHIFT) |
		(((uint32_t)pin_number)  <<  MCI_PROTOCOL_PIN_SERVICE_PIN_NUM_SHIFT)  |
		(MCI_PROTOCOL_PIN_SERVICE_OPTYPE_STATES_GET << MCI_PROTOCOL_PIN_SERVICE_OPTYPE_SHIFT);

	/* send request */
	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_PIN_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX,
		&count, 0x00);

	if (s_code == MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
		/* expect two 32-bits parameters returned from MCU */
		if (count != MCI_PROTOCOL_PIN_STATES_GET_RESULT_COUNT) {
			return MCI_PROTOCOL_STATUS_CODE_FAIL;
		}

		/* verify this is for the same pin */
		field = buffer[0] & MCI_PROTOCOL_PIN_SERVICE_PORT_NUM_MASK;
		field >>= MCI_PROTOCOL_PIN_SERVICE_PORT_NUM_SHIFT;
		if (port_number != (uint8_t)(field & 0xFF)) {
			return MCI_PROTOCOL_STATUS_CODE_FAIL;
		}

		field = buffer[0] & MCI_PROTOCOL_PIN_SERVICE_PIN_NUM_MASK;
		field >>= MCI_PROTOCOL_PIN_SERVICE_PIN_NUM_SHIFT;
		if (pin_number != (uint8_t)(field & 0xFF)) {
			return MCI_PROTOCOL_STATUS_CODE_FAIL;
		}

		/* clear the C structure before filling in returned values */
		memset((void*)pin_statep, 0, sizeof(struct mci_mcu_pin_state_s));

		/* decode returned dir, logic level, muxed function */
		field = buffer[0] & MCI_PROTOCOL_PIN_GPIO_DIRECTION_MASK;
		field >>= MCI_PROTOCOL_PIN_GPIO_DIRECTION_SHIFT;
		pin_statep->dir = (enum mci_mcu_pin_direction_e) field;

		field = buffer[0] & MCI_PROTOCOL_PIN_GPIO_LEVEL_MASK;
		field >>= MCI_PROTOCOL_PIN_GPIO_LEVEL_SHIFT;
		pin_statep->level = (enum mci_mcu_pin_level_e) field;

		field = buffer[1] & MCI_PROTOCOL_PIN_CONFIG_FUNCTION_MUX_MASK;
		field >>= MCI_PROTOCOL_PIN_CONFIG_FUNCTION_MUX_SHIFT;
		pin_statep->mux = (enum mci_mcu_pin_function_e) field;

		/* decode optional fields depends direction */
		if (pin_statep->dir == MCI_MCU_PIN_DIRECTION_OUTPUT) {
			field = buffer[1] & MCI_PROTOCOL_PIN_CONFIG_SLEW_RATE_MASK;
			field >>= MCI_PROTOCOL_PIN_CONFIG_SLEW_RATE_SHIFT;
			pin_statep->params.output.sre = (enum mci_mcu_pin_slew_rate_e) field;

			field = buffer[1] & MCI_PROTOCOL_PIN_CONFIG_DRIVE_STRENGTH_MASK;
			field >>= MCI_PROTOCOL_PIN_CONFIG_DRIVE_STRENGTH_SHIFT;
			pin_statep->params.output.dse = field;
		}
		else {
			field = buffer[1] & MCI_PROTOCOL_PIN_CONFIG_PULL_ENABLE_MASK;
			field >>= MCI_PROTOCOL_PIN_CONFIG_PULL_ENABLE_SHIFT;
			pin_statep->params.input.pe = (bool) field;

			field = buffer[1] & MCI_PROTOCOL_PIN_CONFIG_PULL_SELECT_MASK;
			field >>= MCI_PROTOCOL_PIN_CONFIG_PULL_SELECT_SHIFT;
			pin_statep->params.input.ps = (enum mci_mcu_pin_pull_select_e) field;

			field = buffer[1] & MCI_PROTOCOL_PIN_CONFIG_PASSIVE_FILTER_MASK;
			field >>= MCI_PROTOCOL_PIN_CONFIG_PASSIVE_FILTER_SHIFT;
			pin_statep->params.input.pfe = (bool) field;

			if (pin_statep->params.input.irqc_type != MCI_PIN_IRQ_DISABLED) {
				field = buffer[1] & MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_MASK;
				field >>= MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_SHIFT;
				pin_statep->params.input.irqc_type = (enum mci_pin_irqc_type_e) field;
			}
		}
	}

	swimcu_log(PROT, "%s: status %d, dir %d, level %d\n", __func__, s_code, pin_statep->dir, pin_statep->level);

	return s_code;
}

/************
 *
 * Name:     mci_appl_adc_init
 *
 * Purpose:  To command MCU to initialize ADC module
 *
 * Parms:    configp - pointers to MCU ADC configuration data
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_adc_init(
	struct swimcu *swimcu,
	struct mci_adc_config_s* configp)
{
	enum mci_protocol_status_code_e s_code;
	uint32_t buffer[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	uint32_t bitfield;
	uint8_t  count;
	swimcu_log(ADC, "%s: ADC channel %d\n", __func__, configp->channel);

	/* Mandatory number of 32-bit parameters for the command */
	count = MCI_PROTOCOL_ADC_INIT_ARGS_COUNT_MIN;

	/* encode operation type in the first parameter */
	buffer[0] = (uint32_t) MCI_PROTOCOL_ADC_SERVICE_OPTYPE_INIT;

	/* encode ADCH */
	bitfield   = configp->channel;
	bitfield <<= MCI_PROTOCOL_ADC_CHANNEL_SHIFT;
	bitfield  &= MCI_PROTOCOL_ADC_CHANNEL_MASK;
	buffer[0] |= bitfield;

	/* encode trigger */
	bitfield   = configp->trigger_type;
	bitfield <<= MCI_PROTOCOL_ADC_TRIGGER_SHIFT;
	bitfield  &= MCI_PROTOCOL_ADC_TRIGGER_MASK;
	buffer[0] |= bitfield;

	/* encode the number of samples to be collected for SW average mode */
	if (!configp->hw_average) {
		bitfield   = configp->sample_count;
		bitfield <<= MCI_PROTOCOL_ADC_SAMPLE_COUNT_SHIFT;
		bitfield  &= MCI_PROTOCOL_ADC_SAMPLE_COUNT_MASK;
		buffer[0] |= bitfield;
	}

	/* CFG1: resolution, low power conversion, sample period adjustment enable */
	buffer[1]   = (uint32_t) configp->resolution_mode;
	buffer[1] <<= MCI_PROTOCOL_ADC_RESOLUTION_MODE_SHIFT;
	buffer[1]  &= MCI_PROTOCOL_ADC_RESOLUTION_MODE_MASK;

	bitfield   = (uint32_t) configp->low_power_conv;
	bitfield <<= MCI_PROTOCOL_ADC_LOW_POWER_CONFIG_SHIFT;
	bitfield  &= MCI_PROTOCOL_ADC_LOW_POWER_CONFIG_MASK;
	buffer[1] |= bitfield;

	if (configp->sample_period != MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ADJ_NONE) {
		bitfield   = (uint32_t) configp->sample_period;
		bitfield <<= MCI_PROTOCOL_ADC_SAMPLE_PERIOD_SHIFT;
		bitfield  &= MCI_PROTOCOL_ADC_SAMPLE_PERIOD_MASK;
		buffer[1] |= bitfield;
		buffer[1] |= MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ENABLE_MASK;
	}

	/* CFG2: sample period adjustment (done above), high speed conversion */
	bitfield = (uint32_t) configp->high_speed_conv;
	bitfield <<= MCI_PROTOCOL_ADC_HIGH_SPEED_CONVERSION_SHIFT;
	bitfield  &= MCI_PROTOCOL_ADC_HIGH_SPEED_CONVERSION_MASK;
	buffer[1] |= bitfield;

	/* SC2: ADC trigger mode and HW comparison of ADC results */
	bitfield = (uint32_t) configp->trigger_mode;
	bitfield <<= MCI_PROTOCOL_ADC_TRIGGER_MODE_SHIFT;
	bitfield  &= MCI_PROTOCOL_ADC_TRIGGER_MODE_MASK;
	buffer[1] |= bitfield;

	if (configp->hw_compare.mode != MCI_PROTOCOL_ADC_COMPARE_MODE_DISABLED) {
		/* enable HW comparison */
		buffer[1] |= MCI_PROTOCOL_ADC_HW_COMPARE_ENABLE_MASK;

		/* set V1 value - mandatory */
		buffer[2] = (uint32_t) configp->hw_compare.value1;
		buffer[2] <<= MCI_PROTOCOL_ADC_COMPARE_V1_SHIFT;
		buffer[2]  &= MCI_PROTOCOL_ADC_COMPARE_V1_MASK;
		count++;

		switch (configp->hw_compare.mode) {
			case MCI_PROTOCOL_ADC_COMPARE_MODE_ABOVE:

				/* set grreat than or equal bit */
				buffer[1] |= MCI_PROTOCOL_ADC_HW_COMPARE_GTEQ_MASK;
				break;

			case MCI_PROTOCOL_ADC_COMPARE_MODE_BELOW:

				; /* do nothing */
				break;

			case MCI_PROTOCOL_ADC_COMPARE_MODE_BEYOND:

				/* set grreat than or equal bit */
				buffer[1] |= MCI_PROTOCOL_ADC_HW_COMPARE_GTEQ_MASK;

				/* run through to enable range comparison and set V2 value */

			case MCI_PROTOCOL_ADC_COMPARE_MODE_WITHIN:

				/* enable range comparison */
				buffer[1] |= MCI_PROTOCOL_ADC_HW_COMPARE_RANGE_MASK;

				/* set V2 value */
				bitfield = (uint32_t) configp->hw_compare.value2;
				bitfield <<= MCI_PROTOCOL_ADC_COMPARE_V2_SHIFT;
				bitfield  &= MCI_PROTOCOL_ADC_COMPARE_V2_MASK;
				buffer[2] |= bitfield;

				break;

			default:

				pr_err("%s: Invalid HW compare mode %d", __func__, configp->hw_compare.mode);
				return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
		}
	}
	if (MCI_PROTOCOL_ADC_TRIGGER_MODE_HW == configp->trigger_mode) {
		buffer[3] = configp->trigger_interval;
		count++;
	}

	/* SC3: HW sample average and continuous conversion mode */
	if (configp->hw_average) {
		switch (configp->sample_count) {
			case MCI_ADC_HW_AVERAGE_SAMPLES_4:

				bitfield = (uint32_t) MCI_PROTOCOL_ADC_HW_AVERAGE_COUNT_4;
				break;

			case MCI_ADC_HW_AVERAGE_SAMPLES_8:

				bitfield = (uint32_t) MCI_PROTOCOL_ADC_HW_AVERAGE_COUNT_8;
				break;

			case MCI_ADC_HW_AVERAGE_SAMPLES_16:

				bitfield = (uint32_t) MCI_PROTOCOL_ADC_HW_AVERAGE_COUNT_16;
				break;

			case MCI_ADC_HW_AVERAGE_SAMPLES_32:

				bitfield = (uint32_t) MCI_PROTOCOL_ADC_HW_AVERAGE_COUNT_32;
				break;

			default:

				pr_err("Invalid number of samples requested %d (4,8,16,32) in HW average mode",
					configp->sample_count);
				return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
		}

		bitfield <<= MCI_PROTOCOL_ADC_HW_AVERAGE_SELECT_SHIFT;
		bitfield  &= MCI_PROTOCOL_ADC_HW_AVERAGE_SELECT_MASK;
		buffer[1] |= bitfield;
	}

	/* Expect status code only; no returned results */
	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_ADC_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX,
		&count, 0x00);

	swimcu_log(ADC, "%s: status %d\n", __func__, s_code);

	return s_code;
}

/************
 *
 * Name:     mci_appl_adc_deinit
 *
 * Purpose:  To de-initialize ADC module on ADC
 *
 * Parms:    none
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_adc_deinit(
	struct swimcu *swimcu)
{
	enum mci_protocol_status_code_e s_code;
	uint32_t buffer[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	uint8_t  count;

	count = MCI_PROTOCOL_ADC_DEINIT_ARGS_COUNT;

	buffer[0] = MCI_PROTOCOL_ADC_SERVICE_OPTYPE_DEINIT;

	/* Expect status code only */
	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_ADC_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX,
		&count, 0x00);

	swimcu_log(ADC, "%s: status %d", __func__, s_code);

	return s_code;
}

/************
 *
 * Name:     mci_appl_adc_restart
 *
 * Purpose:  To restart an ADC on a specific channel
 *
 * Parms:    channel - the ADC input channel on which ADC to be started
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_adc_restart(
	struct swimcu *swimcu,
	enum mci_protocol_adc_channel_e   channel)
{
	enum mci_protocol_status_code_e s_code;
	uint32_t buffer[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	uint32_t bitfield;
	uint8_t  count;

	count = MCI_PROTOCOL_ADC_START_ARGS_COUNT;

	buffer[0] = (uint32_t) MCI_PROTOCOL_ADC_SERVICE_OPTYPE_START;

	bitfield   = channel;
	bitfield <<= MCI_PROTOCOL_ADC_CHANNEL_SHIFT;
	bitfield  &= MCI_PROTOCOL_ADC_CHANNEL_MASK;
	buffer[0] |= bitfield;

	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_ADC_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX,
		&count, 0x00);

	/* expect no result values but status code */
	return s_code;
}

/************
 *
 * Name:     mci_appl_adc_read
 *
 * Purpose:  To read ADC value started previously on a specific channel
 *
 * Parms:    channel - the ADC input channel of which the ADC value to be read
 *           adc_valuep - Pointer to storage for returned ADC value.
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_adc_get(
	struct swimcu			*swimcu,
	enum mci_protocol_adc_channel_e	channel,
	uint16_t			*adc_valuep)
{
	enum mci_protocol_status_code_e s_code;
	uint32_t buffer[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	uint32_t bitfield;
	uint8_t  count;

	if (!adc_valuep) {
		return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
	}

	count = MCI_PROTOCOL_ADC_READ_ARGS_COUNT;

	buffer[0] = (uint32_t) MCI_PROTOCOL_ADC_SERVICE_OPTYPE_READ;

	bitfield   = channel;
	bitfield <<= MCI_PROTOCOL_ADC_CHANNEL_SHIFT;
	bitfield  &= MCI_PROTOCOL_ADC_CHANNEL_MASK;
	buffer[0] |= bitfield;

	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_ADC_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX,
		&count, 0x00);

	/* expect single uint32 returned for ADC value if successful */
	if (s_code == MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
		/* verify the channel */
		if (channel == ((buffer[0] & MCI_PROTOCOL_ADC_CHANNEL_MASK)
			>> MCI_PROTOCOL_ADC_CHANNEL_SHIFT)) {
			buffer[0]  &= MCI_PROTOCOL_ADC_VALUE_MASK;
			buffer[0] >>= MCI_PROTOCOL_ADC_VALUE_SHIFT;
			*adc_valuep = (uint16_t) (buffer[0] & 0xFFFF);
		}
		else {
			s_code = MCI_PROTOCOL_STATUS_CODE_CODING_ERROR;
		}
	}

  return s_code;
}

/************
 *
 * Name:     mci_appl_wakeup_source_config
 *
 * Purpose:  To clear a wakeup source on MCU
 *
 * Parms:    configp - pointers to configuration data
 *           optype  - operation type to the wakeup source configuration
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_wakeup_source_config(
	struct swimcu *swimcu,
	struct mci_wakeup_source_config_s         *configp,
	enum mci_protocol_wakeup_source_optype_e  optype)
{
	enum mci_protocol_status_code_e s_code;
	uint32_t buffer[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	uint32_t bitfield;
	uint8_t  count;

	swimcu_log(PROT, "%s: optype=%d source_type=%d", __func__, optype, configp->source_type);

	buffer[0] = (uint32_t) optype;

	bitfield   = configp->source_type;
	bitfield <<= MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_SHIFT;
	bitfield  &= MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_MASK;
	buffer[0] |= bitfield;

	switch (optype) {
		case MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_SET:

			count = MCI_PROTOCOL_WAKEUP_SOURCE_SET_PARAMS_COUNT;
			switch (configp->source_type) {
				case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS:

					swimcu_log(PROT, "PINS type=%d pins=0x%x", configp->source_type, configp->args.pins);
					buffer[1] = configp->args.pins;
					break;

				case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER:

					swimcu_log(PROT, "Timer type=%d timeout=%d", configp->source_type, configp->args.timeout);
					buffer[1] = configp->args.timeout;
					break;

				case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC:

					swimcu_log(PROT, "ADC type=%d adch=%d",
						configp->source_type, configp->args.channel);
					buffer[1] = configp->args.channel;
					break;

				default:

					return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
					break;
			}


			break;

		case MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_GET:
		case MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_CLEAR:

			if (optype == MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_GET) {
				count = MCI_PROTOCOL_WAKEUP_SOURCE_GET_PARAMS_COUNT;
			}
			else {
				count = MCI_PROTOCOL_WAKEUP_SOURCE_CLEAR_PARAMS_COUNT;
			}

			switch (configp->source_type) {
				case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS:

					buffer[1] = configp->args.pins;
					break;

				case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER:

					buffer[1] = 0;
					break;

				case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC:

					/* add additional adc input channel */
					buffer[1]   = configp->args.channel;
					break;

				default:

					return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
					break;
			}
			break;

		default:

			return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
			break;
	}

	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_WAKEUP_SOURCE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX,
		&count, 0x00);

	if ((s_code == MCI_PROTOCOL_STATUS_CODE_SUCCESS) &&
	    (optype == MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_GET)) {
		swimcu_log(PROT, "Returned results %08X %08X ", buffer[0], buffer[1]);
		switch (configp->source_type) {
			case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS:

				configp->args.pins = buffer[1];
				break;

			case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER:

				configp->args.timeout = buffer[1];
				break;

			case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC:

				configp->args.channel = buffer[1];
				break;

			default:

				return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
				break;

		}

	}

	return s_code;
}

/************
 *
 * Name:     swimcu_pm_pwr_off
 *
 * Purpose:  Signal MCU that it is safe to power off
 *
 * Parms:    None
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_pm_pwr_off(struct swimcu *swimcu)
{
	enum mci_protocol_status_code_e s_code;
	uint8_t count = MCI_PROTOCOL_PM_POWER_OFF_SYNC_PARAMS_COUNT;
	uint32_t buffer[MCI_PROTOCOL_PM_POWER_OFF_SYNC_PARAMS_COUNT];

	buffer[0] = (uint32_t)MCI_PROTOCOL_PM_OPTYPE_POWER_OFF_SYNC;

	swimcu_log(PROT, "%s: sending safe shutdown signal",__func__);
	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_PM_SERVICE,
		buffer, 0, &count, 0x00);
	return s_code;
}

/************
 *
 * Name:     swimcu_pm_wait_time_config
 *
 * Purpose:  Specify the time until power off after ULPM is configured
 *
 * Parms:    swimcu            - pointer to the swimcu data structure
 *           wait_sync_time    - time in milliseconds until the MDM sync is complete
 *           wait_pwr_off_time - time in milliseconds until power can be removed after
 *                               sync is complete
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_pm_wait_time_config(
	struct swimcu *swimcu,
	uint32_t wait_sync_time,
	uint32_t wait_pwr_off_time)
{
	enum mci_protocol_status_code_e s_code;
	uint8_t count = MCI_PROTOCOL_PM_POWER_OFF_TIME_CONFIG_PARAMS_COUNT;
	uint32_t buffer[MCI_PROTOCOL_PM_POWER_OFF_TIME_CONFIG_PARAMS_COUNT];

	buffer[0] = (uint32_t)MCI_PROTOCOL_PM_OPTYPE_POWER_OFF_TIME_CONFIG;
	buffer[1] = wait_sync_time;
	buffer[2] = wait_pwr_off_time;

	swimcu_log(PROT, "%s: configuring PM wait sync time %08x %8x %8x\n",
		__func__, buffer[0], buffer[1], buffer[2]);

	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_PM_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX, &count, 0x00);
	return s_code;
}

/************
 *
 * Name:     swimcu_psm_sync_config
 *
 * Purpose:  To config MCU to synchronize power on/off state with MDM
 *
 * Parms:    swimcu       - pointer to the swimcu data structure
 *           sync_option  - selected PSM synchronization option
 *           wait_time    - max wait time in milliseconds for MCU to wait until
 *                          the MDM power-off procedure is complete
 *           mdm_off_time - expected time in milliseconds for MDM in in off state
 *                          (include wait_time)
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_psm_sync_config(
	struct swimcu *swimcu,
	uint32_t sync_option,
	uint32_t wait_time,
	uint32_t mdm_off_time)
{
	enum mci_protocol_status_code_e s_code;
	uint8_t count = MCI_PROTOCOL_PM_PSM_SYNC_CONFIG_PARAMS_COUNT;
	uint32_t buffer[MCI_PROTOCOL_PM_PSM_SYNC_CONFIG_PARAMS_COUNT];

	buffer[0] = sync_option;
	buffer[0] <<= MCI_PROTOCOL_PM_PSM_SYNC_CONFIG_OPTION_SHIFT;
	buffer[0]  &= MCI_PROTOCOL_PM_PSM_SYNC_CONFIG_OPTION_MASK;
	buffer[0]  |= (uint32_t)MCI_PROTOCOL_PM_OPTYPE_PSM_SYNC_CONFIG;

	buffer[1] = wait_time;
	buffer[2] = mdm_off_time;

	swimcu_log(PROT, "%s: configuring PSM synchronization option %08x %8x\n",
		__func__, buffer[0], buffer[1]);

	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_PM_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX, &count, 0x00);
	return s_code;
}

/************
 *
 * Name:     mci_appl_pm_profile_config
 *
 * Purpose:  To encode the profile configuration
 *
 * Parms:    configp - pointers to configuration data
 *           optype  - operation type to the profile configuration
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_pm_profile_config(
	struct swimcu			*swimcu,
	struct mci_pm_profile_config_s 	*configp,
	enum mci_protocol_pm_optype_e	 optype)
{
	enum mci_protocol_status_code_e s_code;
	uint32_t buffer[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	uint32_t bitfield;
	uint8_t  count;

	swimcu_log(PROT, "%s: optype=%d\n", __func__, optype);

	count = MCI_PROTOCOL_OPERATION_PARAMS_COUNT_MIN;
	buffer[0] = (uint32_t) optype;

	switch (optype) {
		case MCI_PROTOCOL_PM_OPTYPE_SET:

			buffer[0] |= ((configp->active_power_mode << MCI_PROTOCOL_PM_ACTIVE_MCU_MODE_SHIFT)
				& MCI_PROTOCOL_PM_ACTIVE_MCU_MODE_MASK);

			buffer[0] |= (configp->active_idle_time << MCI_PROTOCOL_PM_ACTIVE_IDLE_TIME_SHIFT)
				& MCI_PROTOCOL_PM_ACTIVE_IDLE_TIME_MASK;

			buffer[1]   = configp->standby_power_mode;
			buffer[1] <<= MCI_PROTOCOL_PM_STANDBY_MCU_MODE_SHIFT;
			buffer[1]  &= MCI_PROTOCOL_PM_STANDBY_MCU_MODE_MASK;

			bitfield   = configp->standby_mdm_state;
			bitfield <<= MCI_PROTOCOL_PM_STANDBY_MDM_STATE_SHIFT;
			bitfield  &= MCI_PROTOCOL_PM_STANDBY_MDM_STATE_MASK;
			buffer[1] |= bitfield;

			bitfield   = configp->standby_wakeup_sources;
			bitfield <<= MCI_PROTOCOL_PM_STANDBY_WAKEUP_SOURCES_SHIFT;
			bitfield  &= MCI_PROTOCOL_PM_STANDBY_WAKEUP_SOURCES_MASK;
			buffer[1] |= bitfield;

			buffer[2]   = configp->mdm_on_conds_bitset_any;
			buffer[2] <<= MCI_PROTOCOL_MDM_ON_CONDITIONS_ANY_SHIFT;
			buffer[2]  &= MCI_PROTOCOL_MDM_ON_CONDITIONS_ANY_MASK;

			if (configp->mdm_on_conds_bitset_all) {
				bitfield   = configp->mdm_on_conds_bitset_all;
				bitfield <<= MCI_PROTOCOL_MDM_ON_CONDITIONS_ALL_SHIFT;
				bitfield  &= MCI_PROTOCOL_MDM_ON_CONDITIONS_ALL_MASK;
				buffer[2] |= bitfield;
			}

			count = MCI_PROTOCOL_PM_SET_PARAMS_COUNT;
			break;

		case MCI_PROTOCOL_PM_OPTYPE_GET:

			; /* no parameter to encode */
			break;

		default:

			pr_err("%s: Unknown operation type %d", __func__, optype);
			return MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT;
			break;
	}

	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_PM_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX,
		&count, 0x00);

	if ((s_code == MCI_PROTOCOL_STATUS_CODE_SUCCESS) &&
		(optype == MCI_PROTOCOL_PM_OPTYPE_GET)) {
		swimcu_log(PROT, "%s: Returned results %08X %08X %08X\n", __func__, buffer[0], buffer[1], buffer[2]);

		if (count != MCI_PROTOCOL_PM_SET_PARAMS_COUNT) {
			pr_err("%s: Incorrect number of results returned %d (%d)", __func__,
				count, MCI_PROTOCOL_PM_SET_PARAMS_COUNT);
			return MCI_PROTOCOL_STATUS_CODE_ENCODE_ERROR;
		}

		configp->active_power_mode = (enum mci_protocol_power_mode_e)
			((buffer[0] & MCI_PROTOCOL_PM_ACTIVE_MCU_MODE_MASK)
			>> MCI_PROTOCOL_PM_ACTIVE_MCU_MODE_SHIFT);

		configp->active_idle_time = (uint16_t)
			((buffer[0] & MCI_PROTOCOL_PM_ACTIVE_IDLE_TIME_MASK)
			>> MCI_PROTOCOL_PM_ACTIVE_IDLE_TIME_SHIFT);

		configp->standby_power_mode = (enum mci_protocol_power_mode_e)
			((buffer[1] & MCI_PROTOCOL_PM_STANDBY_MCU_MODE_MASK)
			>> MCI_PROTOCOL_PM_STANDBY_MCU_MODE_SHIFT);

		configp->standby_wakeup_sources =
			((buffer[1] & MCI_PROTOCOL_PM_STANDBY_WAKEUP_SOURCES_MASK)
			>> MCI_PROTOCOL_PM_STANDBY_WAKEUP_SOURCES_SHIFT);

		configp->standby_mdm_state = (enum mci_protocol_mdm_state_e)
			((buffer[1] & MCI_PROTOCOL_PM_STANDBY_MDM_STATE_MASK)
			>> MCI_PROTOCOL_PM_STANDBY_MDM_STATE_SHIFT);

		configp->mdm_on_conds_bitset_any = (uint16_t)
			((buffer[2] & MCI_PROTOCOL_MDM_ON_CONDITIONS_ANY_MASK)
			>> MCI_PROTOCOL_MDM_ON_CONDITIONS_ANY_SHIFT);

		configp->mdm_on_conds_bitset_all = (uint16_t)
			((buffer[2] & MCI_PROTOCOL_MDM_ON_CONDITIONS_ALL_MASK)
			>> MCI_PROTOCOL_MDM_ON_CONDITIONS_ALL_SHIFT);

	}

	return s_code;
}

/************
 *
 * Name:     mci_appl_event_query
 *
 * Purpose:  To query events from MCU
 *
 * Parms:    eventp - pointer to an array of event C structure data buffer
 *           countp - pointer to storage for the number of event data buffer
 *                    [IN]  max number of events allowed in input buffer
 *                    [OUT] actual number of retrieved events
 *
 * Return:   MCI_PROTOCOL_STATUS_CODE_SUCCESS if successful;
 *           other status code otherwise.
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
enum mci_protocol_status_code_e swimcu_event_query(
	struct swimcu		*swimcu,
	struct mci_event_s	*eventp,
	int			*countp)
{
	enum mci_protocol_status_code_e s_code;
	uint32_t buffer[MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX];
	int i, count;

	count = 1;
	buffer[0] = (uint32_t) MCI_PROTOCOL_EVENT_SERVICE_OPTYPE_EVENT_QUERY;

	s_code = mci_protocol_command(swimcu, MCI_PROTOCOL_COMMAND_TAG_APPL_EVENT_SERVICE,
		buffer, MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX,
		(uint8_t*)&count, 0x00);
	i = 0;
	if (s_code == MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
		/* decode returned events to available buffer */
		if (count > *countp) {
			count = *countp;
		}

		while (i < count) {
			swimcu_log(EVENT, "%s: retrieved event %d (%d): %08X\n", __func__, i, count, buffer[i]);

			eventp[i].type = (enum mci_protocol_event_type_e)
				((buffer[i] & MCI_PROTOCOL_EVENT_TYPE_MASK)
				>> MCI_PROTOCOL_EVENT_TYPE_SHIFT);

			switch (eventp[i].type) {
				case MCI_PROTOCOL_EVENT_TYPE_GPIO:

					eventp[i].data.gpio_irq.port = (uint8_t)
						((buffer[i] & MCI_PROTOCOL_EVENT_GPIO_PORT_MASK)
						>> MCI_PROTOCOL_EVENT_GPIO_PORT_SHIFT);

					eventp[i].data.gpio_irq.pin = (uint8_t)
						((buffer[i] & MCI_PROTOCOL_EVENT_GPIO_PIN_MASK)
						>> MCI_PROTOCOL_EVENT_GPIO_PIN_SHIFT);

					eventp[i].data.gpio_irq.level = (enum mci_mcu_pin_level_e)
						(buffer[i] & MCI_PROTOCOL_EVENT_GPIO_VALUE_MASK);
					break;

				case MCI_PROTOCOL_EVENT_TYPE_ADC:

					eventp[i].data.adc.adch = (enum mci_protocol_adc_channel_e)
						((buffer[i] & MCI_PROTOCOL_EVENT_ADCH_ID_MASK)
						>> MCI_PROTOCOL_EVENT_ADCH_ID_SHIFT);

					eventp[i].data.adc.value = (uint16_t)
						(buffer[i] & MCI_PROTOCOL_EVENT_ADC_VALUE_MASK);
					break;

				case MCI_PROTOCOL_EVENT_TYPE_RESET:

					eventp[i].data.reset.source = (enum mci_protocol_reset_source_e)
						(buffer[i] & MCI_PROTOCOL_EVENT_RESET_SOURCE_MASK);
					break;

				case MCI_PROTOCOL_EVENT_TYPE_WUSRC:

					eventp[i].data.wusrc.type = (enum mci_protocol_wakeup_source_type_e)
						(buffer[i] & MCI_PROTOCOL_EVENT_WUSRC_TYPE_MASK)
						>> MCI_PROTOCOL_EVENT_WUSRC_TYPE_SHIFT;
					eventp[i].data.wusrc.value = (u16)
						(buffer[i] & MCI_PROTOCOL_EVENT_WUSRC_VALUE_MASK)
						>> MCI_PROTOCOL_EVENT_WUSRC_VALUE_SHIFT;
					break;

				default:
					pr_err("%s: Unknown event[%d] type %d: ", __func__, i, eventp[i].type);
			}

			/* next event */
			i++;
		}
	}
	else {
		count = 0;
		pr_err("%s: fail %d\n", __func__, s_code);
	}

	*countp = count;
	return s_code;
}

