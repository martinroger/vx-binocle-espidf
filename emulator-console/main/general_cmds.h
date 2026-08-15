#pragma once
#include "argtable3/argtable3.h"
#include "driver/gpio.h"
#include "esp_console.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "GENERAL_CMDS"

#pragma region GPIOcontrol HAL

esp_err_t init_GPIO(void) {
	esp_err_t ret = ESP_OK;
	ret = gpio_set_direction((gpio_num_t)CONFIG_ENABLE_5V_PIN, GPIO_MODE_INPUT_OUTPUT);
	ret = gpio_set_pull_mode((gpio_num_t)CONFIG_ENABLE_5V_PIN, GPIO_PULLDOWN_ONLY);
	ret = gpio_pulldown_en((gpio_num_t)CONFIG_ENABLE_5V_PIN);

	return ret;
}

static struct {
	struct arg_int *level;
	struct arg_end *end;
} set_5V_args;

static int set_5V(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&set_5V_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, set_5V_args.end, argv[0]);
		return 1;
	}

	if (set_5V_args.level->count > 1) {
		return 1;
	}
	if ((set_5V_args.level->ival[0] > 1 || set_5V_args.level->ival[0] < 0) && set_5V_args.level->count == 1) {
		printf("Invalid level value\n");
		return 1;
	}

	bool current_5V_level = (bool)gpio_get_level((gpio_num_t)CONFIG_ENABLE_5V_PIN);

	if (set_5V_args.level->count == 0) {
		if (gpio_set_level((gpio_num_t)CONFIG_ENABLE_5V_PIN, !current_5V_level) != ESP_OK) {
			printf("Error toggling 5V enable\n");
			return 1;
		} else {
			printf("5V enable pin toggled to %s\n", !current_5V_level ? "ON" : "OFF");
			return 0;
		}
	} else {

		if (gpio_set_level((gpio_num_t)CONFIG_ENABLE_5V_PIN, set_5V_args.level->ival[0]) != ESP_OK) {
			printf("Issue setting the 5V enable pin to desired level\n");
			return 1;
		} else {
			printf("5V enable pin set to %s\n", (bool)(set_5V_args.level->ival[0]) ? "ON" : "OFF");
			return 0;
		}
	}
}

static void register_set_5V(void) {
	ESP_ERROR_CHECK(init_GPIO());

	set_5V_args.level = arg_int0(NULL, NULL, "<level>", "ON (1) or OFF (0)");
	set_5V_args.end = arg_end(3);

	const esp_console_cmd_t cmd = {.command = "set_5V",
								   .help = "Turn the 5V output on or off",
								   .hint = NULL,
								   .func = &set_5V,
								   .argtable = &set_5V_args};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
