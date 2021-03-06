#ifndef __CALIBRATION_H
#define __CALIBRATION_H

#include <inttypes.h>

#include "clock.h"
#include "control.h"
#include "detection.h"
#include "logging.h"
#include "move.h"

void run_linear_speed_profile(void);
void run_angular_speed_profile(void);
void run_distances_profiling(void);
void run_static_turn_right_profile(void);
void run_front_sensors_calibration(void);

#endif /* __CALIBRATION_H */
