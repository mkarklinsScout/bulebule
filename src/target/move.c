#include "move.h"

static volatile float max_linear_speed = .8;

/* Assume the mouse tail is initially touching a wall */
static float cell_shift = WALL_WIDTH / 2 + MOUSE_TAIL;
static int32_t current_cell_start_micrometers;

void set_starting_position(void)
{
	cell_shift = WALL_WIDTH / 2 + MOUSE_TAIL;
	current_cell_start_micrometers = get_encoder_average_micrometers();
}

float get_max_linear_speed(void)
{
	return max_linear_speed;
}

void set_max_linear_speed(float value)
{
	max_linear_speed = value;
}

/**
 * @brief Mark the beginning of a new cell.
 *
 * It should be executed right after entering a new cell.
 *
 * Takes into account a possible front-wall longitudinal correction.
 */
static void entered_next_cell(void)
{
	int32_t front_wall_correction;

	current_cell_start_micrometers = get_encoder_average_micrometers();
	if (front_wall_detection()) {
		front_wall_correction =
		    (int32_t)((get_front_wall_distance() - CELL_DIMENSION) *
			      MICROMETERS_PER_METER);
		current_cell_start_micrometers += front_wall_correction;
	}
	cell_shift = 0.;
	led_left_toggle();
}

/**
 * @brief Calculate the required micrometers to reach a given speed.
 *
 * This functions assumes the current speed is the target speed and takes into
 * account the configured linear deceleration.
 */
uint32_t required_micrometers_to_speed(float speed)
{
	float acceleration;
	float target_speed = get_target_linear_speed();

	acceleration = (target_speed > speed) ? -get_linear_deceleration()
					      : get_linear_acceleration();

	return (uint32_t)((speed * speed - target_speed * target_speed) /
			  (2 * acceleration) * MICROMETERS_PER_METER);
}

/**
 * @brief Calculate the required time to reach a given speed, in seconds.
 *
 * This functions assumes the current speed is the target speed and takes into
 * account the configured linear deceleration.
 */
float required_time_to_speed(float speed)
{
	float acceleration;
	float target_speed = get_target_linear_speed();

	acceleration = (target_speed > speed) ? -get_linear_deceleration()
					      : get_linear_acceleration();

	return (speed - target_speed) / acceleration;
}

/**
 * @brief Calculate the required ticks to reach a given speed, in ticks.
 *
 * This functions assumes the current speed is the target speed and takes into
 * account the configured linear deceleration.
 */
uint32_t required_ticks_to_speed(float speed)
{
	float required_seconds = required_time_to_speed(speed);

	return (uint32_t)(required_seconds * SYSTICK_FREQUENCY_HZ);
}

/**
 * @brief Activation of sensors control depending on walls around.
 */
static void enable_walls_control(void)
{
	front_sensors_control(front_wall_detection());
	side_sensors_control((right_wall_detection() || left_wall_detection()));
}

/**
 * @brief Disable sensors control.
 */
void disable_walls_control(void)
{
	side_sensors_control(false);
	front_sensors_control(false);
}

/**
 * @brief Accelerate from a starting point and travel a defined distance.
 *
 * The acceleration will always try to reach `max_linear_speed`.
 *
 * @param[in] start Starting point, in micrometers.
 * @param[in] distance Distance to travel, in meters.
 */
void accelerate(int32_t start, float distance)
{
	int32_t target_distance;

	target_distance = start + (int32_t)(distance * MICROMETERS_PER_METER);
	set_target_angular_speed(0.);
	set_target_linear_speed(max_linear_speed);
	while (get_encoder_average_micrometers() < target_distance)
		;
}

/**
 * @brief Decelerate from a starting point and travel a defined distance.
 *
 * @param[in] start Starting point, in micrometers.
 * @param[in] distance Distance to travel, in meters.
 * @param[in] speed Target speed after decelerating, in meters per second.
 */
void decelerate(int32_t start, float distance, float speed)
{
	int32_t target_distance;
	uint32_t target_ticks;

	target_distance = start + (int32_t)(distance * MICROMETERS_PER_METER) -
			  (int32_t)required_micrometers_to_speed(speed);
	set_target_angular_speed(0.);
	set_target_linear_speed(max_linear_speed);
	target_ticks = required_ticks_to_speed(speed);
	while (get_encoder_average_micrometers() < target_distance)
		;
	set_target_linear_speed(speed);
	target_ticks += get_clock_ticks();
	while (get_clock_ticks() < target_ticks)
		;
}

/**
 * @brief Move straight and stop at the end of the current cell.
 */
void stop_end(void)
{
	enable_walls_control();
	decelerate(current_cell_start_micrometers, CELL_DIMENSION, 0.);
	disable_walls_control();
	reset_control_errors();
	entered_next_cell();
}

/**
 * @brief Move straight and stop when the head would touch the front wall.
 */
void stop_head_front_wall(void)
{
	float distance = CELL_DIMENSION - WALL_WIDTH / 2. - MOUSE_HEAD;

	enable_walls_control();
	decelerate(current_cell_start_micrometers, distance, 0.);
	disable_walls_control();
	reset_control_errors();
	cell_shift = distance;
}

/**
 * @brief Move straight and stop at the middle of the current cell.
 */
void stop_middle(void)
{
	float distance = CELL_DIMENSION / 2.;

	enable_walls_control();
	decelerate(current_cell_start_micrometers, distance, 0.);
	disable_walls_control();
	reset_control_errors();
	cell_shift = distance;
}

/**
 * @brief Turn left (90-degree turn with zero linear speed).
 */
void turn_left(void)
{
	uint32_t starting_time = get_clock_ticks();

	set_target_angular_speed(-8 * PI);
	while (get_clock_ticks() - starting_time <= 88)
		;
	set_target_angular_speed(0);
	while (get_clock_ticks() - starting_time <= 176)
		;
}

/**
 * @brief Turn right (90-degree turn with zero linear speed).
 */
void turn_right(void)
{
	uint32_t starting_time = get_clock_ticks();

	set_target_angular_speed(8 * PI);
	while (get_clock_ticks() - starting_time <= 88)
		;
	set_target_angular_speed(0);
	while (get_clock_ticks() - starting_time <= 176)
		;
}

/**
 * @brief Move out of the current cell and into the next cell.
 *
 * This function takes into account the value of the `cell_shift` variable wich
 * is basically used to track the exact position within a cell.
 */
static void move_out(void)
{
	enable_walls_control();
	accelerate(get_encoder_average_micrometers(),
		   CELL_DIMENSION - cell_shift);
	entered_next_cell();
}

/**
 * @brief Move front into the next cell.
 *
 * This function takes into account the value of the `cell_shift` variable wich
 * is basically used to track the exact position within a cell.
 */
void move_front(void)
{
	enable_walls_control();
	accelerate(current_cell_start_micrometers, CELL_DIMENSION - cell_shift);
	entered_next_cell();
}

/**
 * @brief Move left into the next cell.
 */
void move_left(void)
{
	enable_walls_control();
	decelerate(current_cell_start_micrometers, 0.02, 0.666);
	disable_walls_control();
	turn_left();
	enable_walls_control();
	accelerate(get_encoder_average_micrometers(), 0.02);
	entered_next_cell();
}

/**
 * @brief Move right into the next cell.
 */
void move_right(void)
{
	enable_walls_control();
	decelerate(current_cell_start_micrometers, 0.02, 0.666);
	disable_walls_control();
	turn_right();
	enable_walls_control();
	accelerate(get_encoder_average_micrometers(), 0.02);
	entered_next_cell();
}

/**
 * @brief Move back into the previous cell.
 */
void move_back(void)
{
	stop_middle();
	turn_right();
	turn_right();
	move_out();
}

/**
 * @brief Move into the next cell according to a movement direction.
 *
 * @param[in] direction Movement direction.
 */
void move(enum step_direction direction)
{
	if (direction == LEFT)
		move_left();
	else if (direction == RIGHT)
		move_right();
	else if (direction == FRONT)
		move_front();
	else if (direction == BACK)
		move_back();
	else
		stop_middle();
}

/**
 * @brief Reset motion to an iddle state.
 *
 * - Disable motor control.
 * - Disable walls control.
 * - Turn the motor driver off.
 * - Reset control state.
 */
void reset_motion(void)
{
	disable_motor_control();
	disable_walls_control();
	drive_off();
	reset_control_all();
}
