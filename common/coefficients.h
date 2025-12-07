#pragma once
#define COEFF_COOLANT_DEGC_TO_DUTY_M (double)(100.0 / 71.0)
#define COEFF_COOLANT_DEGC_TO_DUTY_P (double)(-100.0 * 64.0 / 71.0)
#define COEFF_DUTY_TO_COOLANT_DEGC_M (double)(71.0 / 100.0)
#define COEFF_DUTY_TO_COOLANT_DEGC_P (double)(64.0)

#define COEFF_RPM_TO_FREQ_M (double)(1.0 / 30.0)
#define COEFF_RPM_TO_FREQ_P (double)(0)
#define COEFF_FREQ_TO_RPM_M (double)(30.0)
#define COEFF_FREQ_TO_RPM_P (double)(0)

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

#define COEFF_LOW_CALIBER_CURRENT (double)9.232   // Sense current for low caliber resistance sensor, in mA
#define COEFF_HIGH_CALIBER_CURRENT (double)0.8992 // Sense current for high caliber resistance sensor, in mA

#define COEFF_FUEL_FULL_R 250.0 // Value for the full tank resistance
#define COEFF_FUEL_FULL_V (COEFF_FUEL_FULL_R * COEFF_LOW_CALIBER_CURRENT / 1000.0) // Voltage value at full tank

#define COEFF_FUEL_PC_TO_V_M (double)(COEFF_FUEL_FULL_V / 100.0) // Conversion scale for fuel percentage to ADC input voltage
#define COEFF_FUEL_PC_TO_V_P (double)(0.0)         // Conversion offset for fuel percentage to ADC input voltage
#define COEFF_FUEL_V_TO_PC_M (double)(100.0 / COEFF_FUEL_FULL_V) // Conversion scale for ADC voltage to fuel percentage
#define COEFF_FUEL_V_TO_PC_P (double)(0.0)         // Conversion offset for ADC voltage to fuel percentage

#define COEFF_LV_TO_V_M (double)(220.0 / 1220.1) // Conversion scale from actual voltage down to ADC readout voltage
#define COEFF_LV_TO_V_P (double)(0.0)            // Conversion offset from actual voltage down to ADC readout voltage
#define COEFF_V_TO_LV_M (double)(1220.1 / 220.0) // Conversion scale from ADC readout voltage up to actual voltage
#define COEFF_V_TO_LV_P (double)(0.0)            // Conversion offset from ADC readout voltage up to actual voltage