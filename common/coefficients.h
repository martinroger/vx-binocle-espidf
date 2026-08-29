#pragma once
#define COEFF_COOLANT_DEGC_TO_DUTY_M (double)(100.0 / 71.0)
#define COEFF_COOLANT_DEGC_TO_DUTY_P (double)(-100.0 * 64.0 / 71.0)
#define COEFF_DUTY_TO_COOLANT_DEGC_M (double)(71.0 / 100.0)
#define COEFF_DUTY_TO_COOLANT_DEGC_P (double)(64.0)

#define COEFF_RPM_TO_FREQ_M (double)(1.0 / 30.0)
#define COEFF_RPM_TO_FREQ_P (double)(0)
#define COEFF_FREQ_TO_RPM_M (double)(30.0)
#define COEFF_FREQ_TO_RPM_P (double)(0)

#define COEFF_MPH_TO_KPH (double)1.609344

#define COEFF_SPEED_KPH_TO_FREQ_M (double)(31285.0 / 7651.0)
#define COEFF_SPEED_KPH_TO_FREQ_P (double)(0.0)
#define COEFF_FREQ_TO_SPEED_KPH_M (double)(7651.0 / 31285.0)
#define COEFF_FREQ_TO_SPEED_KPH_P (double)(0.0)

#define COEFF_PULSES_TO_METER (double)(COEFF_FREQ_TO_SPEED_KPH_M / 3.6) // Conversion factor from pulses to travelled meters

#define COEFF_SPEED_MPH_TO_FREQ_M (double)(17182.0 / 2611.0)
#define COEFF_SPEED_MPH_TO_FREQ_P (double)(0.0)
#define COEFF_FREQ_TO_SPEED_MPH_M (double)(2611.0 / 17182.0)
#define COEFF_FREQ_TO_SPEED_MPH_P (double)(0.0)

#define COEFF_PULSES_TO_MILES (double)(COEFF_FREQ_TO_SPEED_MPH_M / 3600) // Conversion factor from pulses to travelled miles

#define COEFF_K_FACTOR_LOW_SENSE (double)357.85 // Conversion factor to calculate Rsensor when the low caliber is enabled
#define COEFF_K_FACTOR_HI_SENSE (double)3673.88 // Conversion factor to calculate Rsensor when the high caliber is enabled

#define COEFF_FUEL_SWITCH_TO_HI_R (double)300.0 // Threshold in Ohms to switch from Low to High caliber
#define COEFF_FUEL_SWITCH_TO_LOW_R (double)275.0 // Threshold in Ohms to switch from High to Low caliber
#define COEFF_FUEL_OC_R (double)3400.0 // Open-circuit detection threshold in Ohms in High caliber mode
#define COEFF_FUEL_LEARN_MAX_FACTOR (double)1.10 // Max upper boundary factor (10%) for self-learning fuel_full_r
#define COEFF_FUEL_CORRECTION_MULT (double)1.02 // Correction factor to multiply the calculated resistance with to get to the theoretical resistance.
#define COEFF_FUEL_FULL_R 250.0 // Default value for the full tank resistance in Ohms

#define COEFF_VREF_MIN_V (double)3.0 // Minimum acceptable 3V3 reference voltage
#define COEFF_VREF_MAX_V (double)3.6 // Maximum acceptable 3V3 reference voltage
#define COEFF_VREF_DEFAULT_V (double)3.3 // Default 3V3 reference fallback voltage

#define COEFF_LV_TO_V_M (double)(220.0 / 1220.1) // Conversion scale from actual voltage down to ADC readout voltage
#define COEFF_LV_TO_V_P (double)(0.0)            // Conversion offset from actual voltage down to ADC readout voltage
#define COEFF_V_TO_LV_M (double)(1220.1 / 220.0) // Conversion scale from ADC readout voltage up to actual voltage
#define COEFF_V_TO_LV_P (double)(0.0)            // Conversion offset from ADC readout voltage up to actual voltage