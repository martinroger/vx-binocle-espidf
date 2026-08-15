# ESP-IDF MCPWM Design Patterns

- **Pause & Resume**: Keep the hardware timer timebase running continuously. Use generator force levels for pausing (`mcpwm_generator_set_force_level(gen, 0, true)`) and resuming (`mcpwm_generator_set_force_level(gen, -1, true)`).
- **Dynamic Frequency & Duty Updates**: Disable shadow update latching (`update_period_on_empty = false` and `update_cmp_on_tez = false`) when immediate register updates are required without waiting for counter empty/zero transitions.
- **Prescaler Constraints**: Respect the one-prescaler-per-group constraint by allocating channels across Group 0 and Group 1 when distinct resolution clocks are needed.
