#pragma once
#include "argtable3/argtable3.h"
#include "coefficients.h"
#include "esp_console.h"
#include "pwm_gen_helpers.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "PWM_GEN_CMDS"

// Set both duty cycle and frequency for a given channel
static struct {
	struct arg_int *channel;
	struct arg_dbl *duty;
	struct arg_dbl *frequency;
	struct arg_end *end;
} channel_args;

static int set_channel_duty_freq(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&channel_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, channel_args.end, argv[0]);
		return 1;
	}
	assert(channel_args.channel->count == 1);
	assert(channel_args.duty->count == 1);
	assert(channel_args.frequency->count == 1);

	const int target_channel = channel_args.channel->ival[0];
	const double target_duty = channel_args.duty->dval[0];
	const double target_frequency = channel_args.frequency->dval[0];

	// If target duty is 0pc or target frequency is less than 3Hz, pause the channel
	if (target_frequency < 3.0) {
		printf("Target duty or frequency is too low, pausing channel\n");
		pause_channel(target_channel);
		printf("Channel paused.\n");
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused channel...\n");
		resume_channel(target_channel);
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f\n", target_duty);
		return 1;
	}
	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	double actual_duty = get_channel_actual_duty(target_channel);
	double actual_freq = get_channel_actual_freq(target_channel);
	printf("Channel %d set to actual duty %.4f pc at frequency %.4f Hz\n", target_channel, actual_duty, actual_freq);
	return 0;
}

static void register_set_channel_duty_freq(void) {
	channel_args.channel = arg_int1(NULL, NULL, "<chan>", "Channel number (0: Coolant, 1: RPM, 2: Speed)");
	channel_args.duty = arg_dbl1(NULL, NULL, "<d>", "Duty cycle, percentile");
	channel_args.frequency = arg_dbl1(NULL, NULL, "<f>", "Frequency, Hz");
	channel_args.end = arg_end(3);

	const esp_console_cmd_t cmd = {.command = "setChannelDutyFreq",
								   .help = "Set the percentile duty and frequency for a specific channel",
								   .hint = NULL,
								   .func = &set_channel_duty_freq,
								   .argtable = &channel_args};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// Set only duty cycle for a channel
static struct {
	struct arg_int *channel;
	struct arg_dbl *duty;
	struct arg_end *end;
} channel_duty_args;

static int set_channel_duty(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&channel_duty_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, channel_duty_args.end, argv[0]);
		return 1;
	}
	assert(channel_duty_args.channel->count == 1);
	assert(channel_duty_args.duty->count == 1);

	const int target_channel = channel_duty_args.channel->ival[0];
	const double target_duty = channel_duty_args.duty->dval[0];

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused channel...\n");
		resume_channel(target_channel);
	}

	if (target_duty > 100.0 || target_duty < 0.0) {
		printf("Invalid duty cycle, aborting.\n");
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f\n", target_duty);
		return 1;
	}

	double actual_duty = get_channel_actual_duty(target_channel);
	double actual_freq = get_channel_actual_freq(target_channel);
	printf("Channel %d set to actual duty %.4f pc at frequency %.4f Hz\n", target_channel, actual_duty, actual_freq);

	return 0;
}

static void register_set_channel_duty(void) {
	channel_duty_args.channel = arg_int1(NULL, NULL, "<chan>", "Channel number");
	channel_duty_args.duty = arg_dbl1(NULL, NULL, "<d>", "Duty cycle, percentile");
	channel_duty_args.end = arg_end(3);

	const esp_console_cmd_t cmd = {.command = "setChannelDuty",
								   .help = "Set the percentile duty for a specific channel",
								   .hint = NULL,
								   .func = &set_channel_duty,
								   .argtable = &channel_duty_args};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// Set frequency for a given channel
static struct {
	struct arg_int *channel;
	struct arg_dbl *frequency;
	struct arg_end *end;
} channel_freq_args;

static int set_channel_freq(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&channel_freq_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, channel_freq_args.end, argv[0]);
		return 1;
	}
	assert(channel_freq_args.channel->count == 1);
	assert(channel_freq_args.frequency->count == 1);

	const int target_channel = channel_freq_args.channel->ival[0];
	const double target_frequency = channel_freq_args.frequency->dval[0];

	if (target_frequency < 3.0) {
		printf("Target frequency below 3Hz, pausing channel.\n");
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	printf("Channel %d set to frequency %.4f Hz\n", target_channel, actual_freq);

	return 0;
}

static void register_set_channel_freq(void) {
	channel_freq_args.channel = arg_int1(NULL, NULL, "<chan>", "Channel number");
	channel_freq_args.frequency = arg_dbl1(NULL, NULL, "<f>", "Frequency, Hz");
	channel_freq_args.end = arg_end(3);

	const esp_console_cmd_t cmd = {.command = "setChannelFreq",
								   .help = "Set the frequency for a specific channel",
								   .hint = NULL,
								   .func = &set_channel_freq,
								   .argtable = &channel_freq_args};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// PWM generators status
static int getChannelsInfo(int argc, char **argv) {
	for (int i = 0; i < MCPWM_CHANNEL_COUNT; i++) {
		double actual_duty = get_channel_actual_duty(i);
		double actual_freq = get_channel_actual_freq(i);
		bool active = is_channel_active(i);
		printf("Channel %d set to actual duty %.4f pc at frequency %.4f Hz, %s\n", i, actual_duty, actual_freq,
			   active ? "Active" : "Paused");
	}
	return 0;
}

static void register_getChannelsInfo(void) {
	const esp_console_cmd_t cmd = {
		.command = "getChannelsInfo",
		.help = "Get channel metrics",
		.hint = NULL,
		.func = &getChannelsInfo,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// Coolant temperature SET function
#pragma region Coolant temperature
static struct {
	struct arg_dbl *temperature;
	struct arg_end *end;
} setCoolant_args;

static double actual_temperature = 0.0;

static int setCoolant(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&setCoolant_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, setCoolant_args.end, argv[0]);
		return 1;
	}
	assert(setCoolant_args.temperature->count == 1);

	const int target_channel = MCPWM_CHANNEL_COOLANT;
	const double target_frequency = 100.0;

	double target_temperature = setCoolant_args.temperature->dval[0];

	// Run checks on the issued temperature value
	if (target_temperature < 70.0) {
		printf("Target temperature is below 70°C, clamping to minimum value.\n");
		target_temperature = 70.0;
	} else if (target_temperature > 130.0) {
		printf("Target temperature is over 130°C, clamping to maximum value.\n");
		target_temperature = 130.0;
	}

	double target_duty = (double)((target_temperature)*COEFF_COOLANT_DEGC_TO_DUTY_M + COEFF_COOLANT_DEGC_TO_DUTY_P);
	printf("Calculated target duty: %.2f pc\n", target_duty);

	if (target_duty > 100.0 || target_duty < 0.0) {
		printf("Target duty is invalid, aborting.\n");
		return 1;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused coolant channel...\n");
		resume_channel(target_channel);
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f\n", target_duty);
		return 1;
	}
	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	double actual_duty = get_channel_actual_duty(target_channel);
	actual_temperature = COEFF_DUTY_TO_COOLANT_DEGC_M * actual_duty + COEFF_DUTY_TO_COOLANT_DEGC_P;
	double actual_freq = get_channel_actual_freq(target_channel);
	printf("Coolant channel %d set to target duty %.4f pc at frequency %.4f Hz, target temperature %.2f degC, actual "
		   "temperature %.2f degC\n",
		   target_channel, actual_duty, actual_freq, target_temperature, actual_temperature);

	return 0;
}

static void register_setCoolant(void) {
	setCoolant_args.temperature = arg_dbl1(NULL, NULL, "<temperature>", "Float temperature in °C");
	setCoolant_args.end = arg_end(3);

	const esp_console_cmd_t cmd = {.command = "setCoolant",
								   .help = "Set the temperature to the target temperature",
								   .hint = NULL,
								   .func = &setCoolant,
								   .argtable = &setCoolant_args};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

// Get coolant temperature
static int getCoolant(int argc, char **argv) {
	const int target_channel = MCPWM_CHANNEL_COOLANT;

	double actual_duty = get_channel_actual_duty(target_channel);
	actual_temperature = COEFF_DUTY_TO_COOLANT_DEGC_M * actual_duty + COEFF_DUTY_TO_COOLANT_DEGC_P;

	printf("%.2f\n", actual_temperature);
	return 0;
}

static void register_getCoolant(void) {
	const esp_console_cmd_t cmd = {
		.command = "getCoolant",
		.help = "Get coolant temperature (actual)",
		.hint = NULL,
		.func = &getCoolant,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

#pragma endregion

#pragma region RPM
// RPM SET function
static struct {
	struct arg_int *rpm;
	struct arg_end *end;
} setRPM_args, chgRPM_args;

static double current_rpm = 0.0;

static int setRPM(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&setRPM_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, setRPM_args.end, argv[0]);
		return 1;
	}
	assert(setRPM_args.rpm->count == 1);

	const int target_channel = MCPWM_CHANNEL_RPM;
	const double target_duty = 33.0;

	uint32_t target_rpm = (uint32_t)(setRPM_args.rpm->ival[0]);

	// Run checks on the issued rpm value
	if (target_rpm < 250 && target_rpm != 0) {
		printf("Target RPM is below 250, clamping to minimum value.\n");
		target_rpm = 250;
	} else if (target_rpm > 9000) {
		printf("Target RPM is over 9000, clamping to maximum value.\n");
		target_rpm = 9000;
	}

	double target_frequency = (double)target_rpm * COEFF_RPM_TO_FREQ_M + COEFF_RPM_TO_FREQ_P;
	printf("Calculated target frequency: %.4f Hz\n", target_frequency);

	if (target_frequency < 3.0) {
		printf("Target frequency is below 3Hz, pausing RPM channel, actual RPM 0 revs.\n");
		current_rpm = 0.0;
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused RPM channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f !\n", target_duty);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	double actual_RPM = COEFF_FREQ_TO_RPM_M * actual_freq + COEFF_FREQ_TO_RPM_P;
	printf("RPM channel %d set to target duty %.2f pc at actual frequency %.4f Hz, actual RPM %.2f revs\n",
		   target_channel, target_duty, actual_freq, actual_RPM);
	current_rpm = actual_RPM;
	return 0;
}

static int incRPM(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&chgRPM_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, chgRPM_args.end, argv[0]);
		return 1;
	}
	assert(chgRPM_args.rpm->count < 2);

	const int target_channel = MCPWM_CHANNEL_RPM;
	const double target_duty = 33.0;

	// Calculate the target RPM
	uint32_t target_rpm = 0;
	if (chgRPM_args.rpm->count == 1) {
		target_rpm = (uint32_t)round(current_rpm) + (uint32_t)(chgRPM_args.rpm->ival[0]);
	} else {
		target_rpm = (uint32_t)round(current_rpm) + 100;
	}

	// Run checks on the issued rpm value
	if (target_rpm < 250 && target_rpm != 0) {
		printf("Target RPM is below 250, clamping to minimum value.\n");
		target_rpm = 250;
	} else if (target_rpm > 9000) {
		printf("Target RPM is over 9000, clamping to maximum value.\n");
		target_rpm = 9000;
	}

	double target_frequency = (double)target_rpm * COEFF_RPM_TO_FREQ_M + COEFF_RPM_TO_FREQ_P;
	printf("Calculated target frequency: %.4f Hz\n", target_frequency);

	if (target_frequency < 3.0) {
		printf("Target frequency is below 3Hz, pausing RPM channel, actual RPM 0 revs.\n");
		current_rpm = 0.0;
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused RPM channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f !\n", target_duty);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	double actual_RPM = COEFF_FREQ_TO_RPM_M * actual_freq + COEFF_FREQ_TO_RPM_P;
	printf("RPM channel %d set to target duty %.2f pc at actual frequency %.4f Hz, actual RPM %.2f revs\n",
		   target_channel, target_duty, actual_freq, actual_RPM);
	current_rpm = actual_RPM;
	return 0;
}

static int decRPM(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&chgRPM_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, chgRPM_args.end, argv[0]);
		return 1;
	}
	assert(chgRPM_args.rpm->count < 1);

	const int target_channel = MCPWM_CHANNEL_RPM;
	const double target_duty = 33.0;

	// Calculate the target RPM
	uint32_t cur_r = (uint32_t)round(current_rpm);
	uint32_t target_rpm = 0;
	if (chgRPM_args.rpm->count < 2) {
		target_rpm =
			(cur_r > (uint32_t)(chgRPM_args.rpm->ival[0])) ? (cur_r - (uint32_t)(chgRPM_args.rpm->ival[0])) : 0;
	} else {
		target_rpm = (cur_r > 100) ? (cur_r - 100) : 0;
	}

	// Run checks on the issued rpm value
	if (target_rpm < 250 && target_rpm > 0) {
		printf("Target RPM is below 250, clamping to minimum value.\n");
		target_rpm = 250;
	} else if (target_rpm > 9000) {
		printf("Target RPM is over 9000, clamping to maximum value.\n");
		target_rpm = 9000;
	}

	double target_frequency = (double)target_rpm * COEFF_RPM_TO_FREQ_M + COEFF_RPM_TO_FREQ_P;
	printf("Calculated target frequency: %.4f Hz\n", target_frequency);

	if (target_frequency < 3.0) {
		printf("Target frequency is below 3Hz, pausing RPM channel, actual RPM 0 revs.\n");
		current_rpm = 0.0;
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused RPM channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f !\n", target_duty);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	double actual_RPM = COEFF_FREQ_TO_RPM_M * actual_freq + COEFF_FREQ_TO_RPM_P;
	printf("RPM channel %d set to target duty %.2f pc at actual frequency %.4f Hz, actual RPM %.2f revs\n",
		   target_channel, target_duty, actual_freq, actual_RPM);
	current_rpm = actual_RPM;
	return 0;
}

static int getRPM(int argc, char **argv) {
	const int target_channel = MCPWM_CHANNEL_RPM;
	if (is_channel_active(target_channel)) {
		double actual_freq = get_channel_actual_freq(target_channel);
		current_rpm = COEFF_FREQ_TO_RPM_M * actual_freq + COEFF_FREQ_TO_RPM_P;
	} else {
		current_rpm = 0.0;
	}
	printf("%.2f\n", current_rpm);
	return 0;
}

static void register_getRPM(void) {
	const esp_console_cmd_t cmd = {
		.command = "getRPM",
		.help = "Get RPM (actual)",
		.hint = NULL,
		.func = &getRPM,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_setRPM(void) {
	setRPM_args.rpm = arg_int1(NULL, NULL, "<rpm>", "Revolutions per minute");
	setRPM_args.end = arg_end(3);

	chgRPM_args.rpm = arg_int0(NULL, NULL, "<rpm_delta>", "Amount by which to change the RPM target");
	chgRPM_args.end = arg_end(3);

	const esp_console_cmd_t setCmd = {.command = "setRPM",
									  .help = "Set RPM value (beware of quantization)",
									  .hint = NULL,
									  .func = &setRPM,
									  .argtable = &setRPM_args};

	const esp_console_cmd_t incCmd = {.command = "incRPM",
									  .help = "Increase RPM value (beware of quantization)",
									  .hint = NULL,
									  .func = &incRPM,
									  .argtable = &chgRPM_args};

	const esp_console_cmd_t decCmd = {.command = "decRPM",
									  .help = "Decrease RPM value (beware of quantization)",
									  .hint = NULL,
									  .func = &decRPM,
									  .argtable = &chgRPM_args};

	ESP_ERROR_CHECK(esp_console_cmd_register(&setCmd));
	ESP_ERROR_CHECK(esp_console_cmd_register(&incCmd));
	ESP_ERROR_CHECK(esp_console_cmd_register(&decCmd));
}

#pragma endregion

// Set Speed function
#define MAX_SPEED_KPH 271.0
#define MAX_SPEED_MPH 168.0
static struct {
	struct arg_dbl *speed;
	struct arg_end *end;
} setSpeed_args, chgSpeed_args;

static double current_speed_kph = 0.0;
static double current_speed_mph = 0.0;

#pragma region KPH Speed

static int setSpeedKPH(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&setSpeed_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, setSpeed_args.end, argv[0]);
		return 1;
	}
	assert(setSpeed_args.speed->count == 1);

	const int target_channel = MCPWM_CHANNEL_SPEED;
	const double target_duty = 33.0;

	double target_speed = setSpeed_args.speed->dval[0];

	// Run checks on the issued speed value
	if (target_speed > MAX_SPEED_KPH) {
		printf("Target Speed is over %.0f, clamping to maximum value.\n", MAX_SPEED_KPH);
		target_speed = MAX_SPEED_KPH;
	}
	if (target_speed < 0.0) {
		printf("Invalid target speed, aborting\n");
		return 1;
	}

	double target_frequency = target_speed * COEFF_SPEED_KPH_TO_FREQ_M + COEFF_SPEED_KPH_TO_FREQ_P;
	printf("Calculated target frequency: %.4f Hz\n", target_frequency);

	if (target_frequency < 3.0) {
		printf("Target frequency is below 3Hz, pausing speed channel, expected speed 0 kph.\n");
		current_speed_kph = 0.0;
		current_speed_mph = 0.0;
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused speed channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f !\n", target_duty);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	double actual_speed = (COEFF_FREQ_TO_SPEED_KPH_M * actual_freq + COEFF_FREQ_TO_SPEED_KPH_P);
	printf("Speed channel %d set to target duty %.2f pc at actual frequency %.4f Hz, expected speed %.2f kph\n",
		   target_channel, target_duty, actual_freq, actual_speed);
	current_speed_kph = actual_speed;
	current_speed_mph = actual_speed / 1.60934;
	return 0;
}

static int incSpeedKPH(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&chgSpeed_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, chgSpeed_args.end, argv[0]);
		return 1;
	}
	assert(chgSpeed_args.speed->count < 2);

	const int target_channel = MCPWM_CHANNEL_SPEED;
	const double target_duty = 33.0;

	double target_speed = 0.0;
	if (chgSpeed_args.speed->count > 0) {
		target_speed = current_speed_kph + (chgSpeed_args.speed->dval[0]);
	} else {
		target_speed = current_speed_kph + 5.0;
	}

	// Run checks on the issued speed value
	if (target_speed > MAX_SPEED_KPH) {
		printf("Target Speed is over %.0f, clamping to maximum value.\n", MAX_SPEED_KPH);
		target_speed = MAX_SPEED_KPH;
	}
	if (target_speed < 0.0) {
		printf("Negative speed, clamping to 0\n");
		target_speed = 0.0;
	}

	double target_frequency = target_speed * COEFF_SPEED_KPH_TO_FREQ_M + COEFF_SPEED_KPH_TO_FREQ_P;
	printf("Calculated target frequency: %.4f Hz\n", target_frequency);

	if (target_frequency < 3.0) {
		printf("Target frequency is below 3Hz, pausing speed channel, expected speed 0 kph.\n");
		current_speed_kph = 0.0;
		current_speed_mph = 0.0;
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused speed channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f !\n", target_duty);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	double actual_speed = (COEFF_FREQ_TO_SPEED_KPH_M * actual_freq + COEFF_FREQ_TO_SPEED_KPH_P);
	printf("Speed channel %d set to target duty %.2f pc at actual frequency %.4f Hz, expected speed %.2f kph\n",
		   target_channel, target_duty, actual_freq, actual_speed);
	current_speed_kph = actual_speed;
	current_speed_mph = actual_speed / 1.60934;
	return 0;
}

static int decSpeedKPH(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&chgSpeed_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, chgSpeed_args.end, argv[0]);
		return 1;
	}
	assert(chgSpeed_args.speed->count < 2);

	const int target_channel = MCPWM_CHANNEL_SPEED;
	const double target_duty = 33.0;

	double target_speed = 0.0;
	if (chgSpeed_args.speed->count > 0) {
		target_speed = (current_speed_kph > chgSpeed_args.speed->dval[0])
						   ? (current_speed_kph - chgSpeed_args.speed->dval[0])
						   : 0.0;
	} else {
		target_speed = (current_speed_kph > 5.0) ? (current_speed_kph - 5.0) : 0.0;
	}

	// Run checks on the issued speed value
	if (target_speed > MAX_SPEED_KPH) {
		printf("Target Speed is over %.0f, clamping to maximum value.\n", MAX_SPEED_KPH);
		target_speed = MAX_SPEED_KPH;
	}
	if (target_speed < 0.0) {
		printf("Negative speed, clamping to 0\n");
		target_speed = 0.0;
	}

	double target_frequency = target_speed * COEFF_SPEED_KPH_TO_FREQ_M + COEFF_SPEED_KPH_TO_FREQ_P;
	printf("Calculated target frequency: %.4f Hz\n", target_frequency);

	if (target_frequency < 3.0) {
		printf("Target frequency is below 3Hz, pausing speed channel, expected speed 0 kph.\n");
		current_speed_kph = 0.0;
		current_speed_mph = 0.0;
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused speed channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f !\n", target_duty);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	double actual_speed = (COEFF_FREQ_TO_SPEED_KPH_M * actual_freq + COEFF_FREQ_TO_SPEED_KPH_P);
	printf("Speed channel %d set to target duty %.2f pc at actual frequency %.4f Hz, expected speed %.2f kph\n",
		   target_channel, target_duty, actual_freq, actual_speed);
	current_speed_kph = actual_speed;
	current_speed_mph = actual_speed / 1.60934;
	return 0;
}

static void register_setSpeedKPH(void) {
	setSpeed_args.speed = arg_dbl1(NULL, NULL, "<speed>", "Kilometers per hour");
	setSpeed_args.end = arg_end(3);

	chgSpeed_args.speed = arg_dbl0(NULL, NULL, "<speed_delta>", "KPH to add/remove");
	chgSpeed_args.end = arg_end(3);

	const esp_console_cmd_t cmd = {.command = "setSpeedKPH",
								   .help = "Set kph speed value (beware of quantization)",
								   .hint = NULL,
								   .func = &setSpeedKPH,
								   .argtable = &setSpeed_args};

	const esp_console_cmd_t incCmd = {.command = "incSpeedKPH",
									  .help = "Increase kph speed value (beware of quantization)",
									  .hint = NULL,
									  .func = &incSpeedKPH,
									  .argtable = &chgSpeed_args};

	const esp_console_cmd_t decCmd = {.command = "decSpeedKPH",
									  .help = "Decrease kph speed value (beware of quantization)",
									  .hint = NULL,
									  .func = &decSpeedKPH,
									  .argtable = &chgSpeed_args};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
	ESP_ERROR_CHECK(esp_console_cmd_register(&incCmd));
	ESP_ERROR_CHECK(esp_console_cmd_register(&decCmd));
}

#pragma endregion

#pragma region MPH Speed

static int setSpeedMPH(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&setSpeed_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, setSpeed_args.end, argv[0]);
		return 1;
	}
	assert(setSpeed_args.speed->count == 1);

	const int target_channel = MCPWM_CHANNEL_SPEED;
	const double target_duty = 33.0;

	double target_speed = setSpeed_args.speed->dval[0];

	// Run checks on the issued speed value
	if (target_speed > MAX_SPEED_MPH) {
		printf("Target Speed is over %.0f, clamping to maximum value.\n", MAX_SPEED_MPH);
		target_speed = MAX_SPEED_MPH;
	}
	if (target_speed < 0.0) {
		printf("Invalid target speed, aborting\n");
		return 1;
	}

	double target_frequency = target_speed * COEFF_SPEED_MPH_TO_FREQ_M + COEFF_SPEED_MPH_TO_FREQ_P;
	printf("Calculated target frequency: %.4f Hz\n", target_frequency);

	if (target_frequency < 3.0) {
		printf("Target frequency is below 3Hz, pausing speed channel, expected speed 0 mph.\n");
		current_speed_kph = 0.0;
		current_speed_mph = 0.0;
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused speed channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f !\n", target_duty);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	double actual_speed = (COEFF_FREQ_TO_SPEED_MPH_M * actual_freq + COEFF_FREQ_TO_SPEED_MPH_P);
	printf("Speed channel %d set to target duty %.2f pc at actual frequency %.4f Hz, expected speed %.2f mph\n",
		   target_channel, target_duty, actual_freq, actual_speed);
	current_speed_mph = actual_speed;
	current_speed_kph = actual_speed * 1.60934;
	return 0;
}

static int incSpeedMPH(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&chgSpeed_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, chgSpeed_args.end, argv[0]);
		return 1;
	}
	assert(chgSpeed_args.speed->count < 2);

	const int target_channel = MCPWM_CHANNEL_SPEED;
	const double target_duty = 33.0;

	double target_speed = 0.0;
	if (chgSpeed_args.speed->count > 0) {
		target_speed = current_speed_mph + (chgSpeed_args.speed->dval[0]);
	} else {
		target_speed = current_speed_mph + 3.0;
	}

	// Run checks on the issued speed value
	if (target_speed > MAX_SPEED_MPH) {
		printf("Target Speed is over %.0f, clamping to maximum value.\n", MAX_SPEED_MPH);
		target_speed = MAX_SPEED_MPH;
	}
	if (target_speed < 0.0) {
		printf("Negative speed, clamping to 0\n");
		target_speed = 0.0;
	}

	double target_frequency = target_speed * COEFF_SPEED_MPH_TO_FREQ_M + COEFF_SPEED_MPH_TO_FREQ_P;
	printf("Calculated target frequency: %.4f Hz\n", target_frequency);

	if (target_frequency < 3.0) {
		printf("Target frequency is below 3Hz, pausing speed channel, expected speed 0 mph.\n");
		current_speed_kph = 0.0;
		current_speed_mph = 0.0;
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused speed channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f !\n", target_duty);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	double actual_speed = (COEFF_FREQ_TO_SPEED_MPH_M * actual_freq + COEFF_FREQ_TO_SPEED_MPH_P);
	printf("Speed channel %d set to target duty %.2f pc at actual frequency %.4f Hz, expected speed %.2f mph\n",
		   target_channel, target_duty, actual_freq, actual_speed);
	current_speed_mph = actual_speed;
	current_speed_kph = actual_speed * 1.60934;
	return 0;
}

static int decSpeedMPH(int argc, char **argv) {
	int nerrors = arg_parse(argc, argv, (void **)&chgSpeed_args);
	if (nerrors != 0) {
		arg_print_errors(stderr, chgSpeed_args.end, argv[0]);
		return 1;
	}
	assert(chgSpeed_args.speed->count < 2);

	const int target_channel = MCPWM_CHANNEL_SPEED;
	const double target_duty = 33.0;

	double target_speed = 0.0;
	if (chgSpeed_args.speed->count > 0) {
		target_speed = (current_speed_mph > chgSpeed_args.speed->dval[0])
						   ? (current_speed_mph - chgSpeed_args.speed->dval[0])
						   : 0.0;
	} else {
		target_speed = (current_speed_mph > 3.0) ? (current_speed_mph - 3.0) : 0.0;
	}

	// Run checks on the issued speed value
	if (target_speed > MAX_SPEED_MPH) {
		printf("Target Speed is over %.0f, clamping to maximum value.\n", MAX_SPEED_MPH);
		target_speed = MAX_SPEED_MPH;
	}
	if (target_speed < 0.0) {
		printf("Negative speed, clamping to 0\n");
		target_speed = 0.0;
	}

	double target_frequency = target_speed * COEFF_SPEED_MPH_TO_FREQ_M + COEFF_SPEED_MPH_TO_FREQ_P;
	printf("Calculated target frequency: %.4f Hz\n", target_frequency);

	if (target_frequency < 3.0) {
		printf("Target frequency is below 3Hz, pausing speed channel, expected speed 0 mph.\n");
		current_speed_kph = 0.0;
		current_speed_mph = 0.0;
		pause_channel(target_channel);
		return 0;
	}

	if (!is_channel_active(target_channel)) {
		printf("Resuming paused speed channel...\n");
		resume_channel(target_channel);
	}

	if (change_frequency(target_channel, target_frequency) != ESP_OK) {
		printf("Cannot change frequency to %.4f\n", target_frequency);
		return 1;
	}

	if (change_duty_cycle(target_channel, target_duty) != ESP_OK) {
		printf("Cannot change duty cycle to %.2f !\n", target_duty);
		return 1;
	}

	double actual_freq = get_channel_actual_freq(target_channel);
	double actual_speed = (COEFF_FREQ_TO_SPEED_MPH_M * actual_freq + COEFF_FREQ_TO_SPEED_MPH_P);
	printf("Speed channel %d set to target duty %.2f pc at actual frequency %.4f Hz, expected speed %.2f mph\n",
		   target_channel, target_duty, actual_freq, actual_speed);
	current_speed_mph = actual_speed;
	current_speed_kph = actual_speed * 1.60934;
	return 0;
}

static void register_setSpeedMPH(void) {
	setSpeed_args.speed = arg_dbl1(NULL, NULL, "<speed>", "Miles per hour");
	setSpeed_args.end = arg_end(3);

	chgSpeed_args.speed = arg_dbl0(NULL, NULL, "<speed_delta>", "Miles per hour");
	chgSpeed_args.end = arg_end(3);

	const esp_console_cmd_t cmd = {.command = "setSpeedMPH",
								   .help = "Set mph speed value (beware of quantization)",
								   .hint = NULL,
								   .func = &setSpeedMPH,
								   .argtable = &setSpeed_args};

	const esp_console_cmd_t incCmd = {.command = "incSpeedMPH",
									  .help = "Increase mph speed value (beware of quantization)",
									  .hint = NULL,
									  .func = &incSpeedMPH,
									  .argtable = &chgSpeed_args};

	const esp_console_cmd_t decCmd = {.command = "decSpeedMPH",
									  .help = "Decrease mph speed value (beware of quantization)",
									  .hint = NULL,
									  .func = &decSpeedMPH,
									  .argtable = &chgSpeed_args};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
	ESP_ERROR_CHECK(esp_console_cmd_register(&incCmd));
	ESP_ERROR_CHECK(esp_console_cmd_register(&decCmd));
}

#pragma endregion
#pragma region getSpeed
// getSpeed

static int getSpeed(int argc, char **argv) {
	const int target_channel = MCPWM_CHANNEL_SPEED;
	if (is_channel_active(target_channel)) {
		double actual_freq = get_channel_actual_freq(target_channel);
		double actual_speed_kph = (COEFF_FREQ_TO_SPEED_KPH_M * actual_freq + COEFF_FREQ_TO_SPEED_KPH_P);
		double actual_speed_mph = (COEFF_FREQ_TO_SPEED_MPH_M * actual_freq + COEFF_FREQ_TO_SPEED_MPH_P);
		current_speed_kph = actual_speed_kph;
		current_speed_mph = actual_speed_mph;
	} else {
		current_speed_kph = 0.0;
		current_speed_mph = 0.0;
	}
	printf("%.2f|%.2f\n", current_speed_kph, current_speed_mph);
	return 0;
}

static void register_getSpeed(void) {
	const esp_console_cmd_t cmd = {
		.command = "getSpeed",
		.help = "Get speeds (actual)",
		.hint = NULL,
		.func = &getSpeed,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

#pragma endregion