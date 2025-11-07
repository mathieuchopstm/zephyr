#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/instrumentation/instrumentation.h>
#include <instr_buffer.h>
#include <instr_timestamp.h>

__no_instrumentation__
static int cmd_idump(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t *transferring_buf;
	uint32_t transferring_length, instr_buffer_max_length;

	/* Make sure instrumentation is disabled. */
	instr_disable();

	/* Initiator mark */
	printk("-*-#");

	instr_buffer_max_length = instr_buffer_capacity_get();

	while (!instr_buffer_is_empty()) {
		transferring_length =
			instr_buffer_get_claim(
					&transferring_buf,
					instr_buffer_max_length);

		for (uint32_t i = 0; i < transferring_length; i++) {
			shell_fprintf(sh, SHELL_NORMAL, "%c", transferring_buf[i]);
		}

		instr_buffer_get_finish(transferring_length);
	}

	/* Terminator mark */
	printk("-*-!\n");

	return 0;
}

SHELL_CMD_REGISTER(idump, NULL, "Dump instrumentation trace\n", cmd_idump);
