// Copyright (c) 2020 by Gilbert Ramirez <gram@alumni.rice.edu>
//
// v8
//
// This program controls the tail lights of a car.
//
// Each tail light, both left and right, is comprised of a sequence
// of 4 LEDs.  As inputs, the program honors the left turn signal, the
// right turn signal, the hazard light switch, and the brake light.
//
// The turn signals are animiations of sequenced LED lights.
// Turning on the brakes also results in an animation.
//
// This program does not deal with daytime running lights.

#define USE_SERIAL_PRINT false

// Analog Inputs
int SWITCH_BRAKE = A0;
int SWITCH_LEFT_TURN = A1;
int SWITCH_RIGHT_TURN = A2;
int SWITCH_HAZARD = A3;

// Left-hand side LEDs
int LEFT_LED0 = 4;
int LEFT_LED1 = 5;
int LEFT_LED2 = 6;
int LEFT_LED3 = 7;

// Right-hand side LEDs
int RIGHT_LED0 = 8;
int RIGHT_LED1 = 9;
int RIGHT_LED2 = 10;
int RIGHT_LED3 = 11;

class Switches {
  static constexpr int analog_cutoff = 1000;

  public:
	bool brake;
	bool left;
	bool right;
	bool hazard;

	void evaluate() {
		brake = analogRead(SWITCH_BRAKE) > analog_cutoff;
		left = analogRead(SWITCH_LEFT_TURN) > analog_cutoff;
		right = analogRead(SWITCH_RIGHT_TURN) > analog_cutoff;
		hazard = analogRead(SWITCH_HAZARD) > analog_cutoff;
	}
};

// For a set of LEDs in a single tail light, define the state of all LEDS
// for a single step in the animation.
struct animation_pattern {
	int led0;
	int led1;
	int led2;
	int led3;
};

// The animation consits of this sequence of LED stpes
struct animation_pattern animation_steps[] = {
	// LED0, LED1, LED2, LED3
	// This 0th step must be "all off"
	{ LOW, LOW, LOW, LOW },
	// This 1st step is where the animations start
	{ HIGH, LOW, LOW, LOW },
	{ HIGH, HIGH, LOW, LOW },
	{ HIGH, HIGH, HIGH, LOW },
	{ HIGH, HIGH, HIGH, HIGH },
	// After this step, the animation cycles to the 0th step
};

constexpr int num_animation_steps = sizeof animation_steps / sizeof animation_steps[0];

// 2 arbitary numbers to indicate left or right
constexpr int SIDE_LEFT = 501;
constexpr int SIDE_RIGHT = 502;

// This holds the state of one tail light
class TailLight {
	// The entire system of lights is controlled by a state machines for each
	// tail light. The states are:
	static constexpr int STATE_ALL_OFF_STEADY = 100;
	static constexpr int STATE_TURN_ANIMATION = 101;
	static constexpr int STATE_HAZARD_ANIMATION = 102;
	static constexpr int STATE_BRAKE_PRE_STEADY_ANIMATION = 103;
	static constexpr int STATE_BRAKE_ALL_ON_STEADY = 104;

	// How many milliseconds between updating the animations. The turn/hazard
	// animations can have a different speed from the brake animations.
	static constexpr int TH_ANIMATION_STEP_DELAY_MSEC = 200;
	static constexpr int BR_ANIMATION_STEP_DELAY_MSEC = 150;

	// These are the LED pins for this tail light
	int led0 = -1;
	int led1 = -1;
	int led2 = -1;
	int led3 = -1;

	int state = STATE_ALL_OFF_STEADY;
	// when animating is true, the code will always update the LEDs
	// to display the animation_step. Whe animating is false, the LEDs
	// are not changed
	bool animating = false;
	int animation_step = 0;
	int step_delay_ms = 0;
	int clock_ms = 0;

	// Is this TailLight the LEFT or RIGHT one?
	int side = -1;
	
  public:

	TailLight( int side_, int led0_, int led1_, int led2_, int led3_ )
		: side( side_ )
		, led0(led0_)
		, led1(led1_)
		, led2(led2_)
		, led3(led3_)
		{}

	void setup() {
		pinMode(led0, OUTPUT);
		pinMode(led1, OUTPUT);
		pinMode(led2, OUTPUT);
		pinMode(led3, OUTPUT);
	}

	void all_off() {
		digitalWrite(led0, LOW);
		digitalWrite(led1, LOW);
		digitalWrite(led2, LOW);
		digitalWrite(led3, LOW);
	}

	void all_on() {
		digitalWrite(led0, HIGH);
		digitalWrite(led1, HIGH);
		digitalWrite(led2, HIGH);
		digitalWrite(led3, HIGH);
	}

	// reset and enable animation
	void enable_animation( int delay_ms ) {
		animating = true;
		// start at 1 instead of 0, to see an immediate change
		// (assuming the 0th step is all off!)
		animation_step = 1;

		step_delay_ms = delay_ms;
		clock_ms = 0;
	}

	// Change the current animaition
	void change_animation( int delay_ms ) {
		step_delay_ms = delay_ms;
	}

	void disable_animation() {
		animating = false;
		animation_step = 0;
		step_delay_ms = 0;
		clock_ms = 0;
	}

	// Update the clock and move possibly move the animation step forward,
	// if we are in animation mode.
	void tick( int time_delta ) {
		if (! animating) {
			return;
		}
		clock_ms += time_delta;
		if ( clock_ms >= step_delay_ms ) {
			animation_step += 1;
			clock_ms = 0;
		}
	}

	void animate() {
		if ( animation_step >= num_animation_steps ) {
			animation_step = 0;
		}
		auto pattern = animation_steps[animation_step];
		digitalWrite(led0, pattern.led0);
		digitalWrite(led1, pattern.led1);
		digitalWrite(led2, pattern.led2);
		digitalWrite(led3, pattern.led3);
	}

	void loop(Switches& switches) {
		switch (state) {
		case STATE_ALL_OFF_STEADY:
			analyze_state_all_off_steady(switches);
			break;
		case STATE_TURN_ANIMATION:
			analyze_state_turn_animation(switches);
			break;
		case STATE_HAZARD_ANIMATION:
			analyze_state_hazard_animation(switches);
			break;
		case STATE_BRAKE_PRE_STEADY_ANIMATION:
			analyze_state_brake_pre_steady_animation(switches);
			break;
		case STATE_BRAKE_ALL_ON_STEADY:
			analyze_state_brake_all_on_steady(switches);
			break;
		}

		if ( USE_SERIAL_PRINT ) {
			if (side == SIDE_LEFT) {
				Serial.print("L ");
			} else {
				Serial.print("R ");
			}

			Serial.print(clock_ms);
			Serial.print(" ");
			Serial.print("state=");
			Serial.print(state);
			Serial.print(" ");
			Serial.print("animating=");
			Serial.print(animating);
			Serial.print(" ");
			Serial.print("animation_step=");
			Serial.print(animation_step);
			Serial.print(" ");
			Serial.print("\n");
		}

		if (animating) {
			animate();
		}
	}

	void analyze_state_all_off_steady(Switches& switches) {
		bool can_honor_brake = false;
		// The hazard indicator supercedes either turn signal
		if ( switches.hazard ) {
			state = STATE_HAZARD_ANIMATION;
			enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
			return;
		}
		else if ( switches.left ) {
			// We turn on if we are the left light
			if ( side == SIDE_LEFT ) {
				state = STATE_TURN_ANIMATION;
				enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
				return;
			}
			else {
				can_honor_brake = true;
			}
		}
		else if ( switches.right ) {
			// We turn on if we are the right light
			if ( side == SIDE_RIGHT ) {
				state = STATE_TURN_ANIMATION;
				enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
				return;
			}
			else {
				can_honor_brake = true;
			}
		}
		else {
			// Maybe the brake switch is on
			can_honor_brake = true;
		}

		// Check the brake only if this light can honor the brake
		if ( can_honor_brake && switches.brake ) {
			state = STATE_BRAKE_PRE_STEADY_ANIMATION;
			enable_animation( BR_ANIMATION_STEP_DELAY_MSEC );
			return;
		}
		else {
			// no change
			return;
		}
	}

	void analyze_state_hazard_animation(Switches& switches) {
		bool can_honor_brake = false;
		// The hazard indicator supercedes either turn signal
		if ( switches.hazard ) {
			// no change
			return;
		}
		else if ( switches.left ) {
			// We turn on if we are the left light
			if ( side == SIDE_LEFT ) {
				state = STATE_TURN_ANIMATION;
				enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
				return;
			}
			else {
				can_honor_brake = true;
			}
		}
		else if ( switches.right ) {
			// We turn on if we are the right light
			if ( side == SIDE_RIGHT ) {
				state = STATE_TURN_ANIMATION;
				enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
				return;
			}
			else {
				can_honor_brake = true;
			}
		}
		else {
			// Maybe the brake switch is on
			can_honor_brake = true;
		}

		// Check the brake only if this light can honor the brake
		if ( can_honor_brake && switches.brake ) {
			state = STATE_BRAKE_PRE_STEADY_ANIMATION;
			enable_animation( BR_ANIMATION_STEP_DELAY_MSEC );
			return;
		}
		else {
			state = STATE_ALL_OFF_STEADY;
			disable_animation();
			all_off();
			return;
		}
	}


	void analyze_state_turn_animation(Switches& switches) {
		bool can_honor_brake = false;
		// The hazard indicator supercedes either turn signal
		if ( switches.hazard ) {
			state = STATE_HAZARD_ANIMATION;
			enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
			return;
		}
		else if ( switches.left ) {
			if ( side == SIDE_LEFT ) {
				// no change
				return;
			}
			else {
				can_honor_brake = true;
			}

		}
		else if ( switches.right ) {
			if ( side == SIDE_RIGHT ) {
				// no change;
				return;
			}
			else {
				can_honor_brake = true;
			}
		}
		else {
			// Maybe the brake switch is on
			can_honor_brake = true;
		}

		// Check the brake only if this light can honor the brake
		if ( can_honor_brake && switches.brake ) {
			state = STATE_BRAKE_PRE_STEADY_ANIMATION;
			enable_animation( BR_ANIMATION_STEP_DELAY_MSEC );
			return;
		}
		else {
			state = STATE_ALL_OFF_STEADY;
			disable_animation();
			all_off();
			return;
		}
	}

	void analyze_state_brake_pre_steady_animation(Switches& switches) {
		// The hazard indicator supercedes everything
		if ( switches.hazard ) {
			state = STATE_HAZARD_ANIMATION;
			enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
			return;
		}
		// Before checking the brakes, check if this light needs
		// to change to a turn indicator.
		else if ( switches.left && side == SIDE_LEFT ) {
			state = STATE_TURN_ANIMATION;
			// Proceed from the current animation step to the new one
			change_animation( TH_ANIMATION_STEP_DELAY_MSEC );
			return;
		}
		else if ( switches.right && side == SIDE_RIGHT ) {
			state = STATE_TURN_ANIMATION;
			// Proceed from the current animation step to the new one
			change_animation( TH_ANIMATION_STEP_DELAY_MSEC );
			return;
		}

		if ( switches.brake ) {
			// If we have gone beyond the number of animation steps,
			// and the next animation step would loop back,
			// go to the next state
			if ( animation_step >= num_animation_steps ) {
				state = STATE_BRAKE_ALL_ON_STEADY;
				disable_animation();
				all_on();
				return;
			}
			else {
				// no change
				return;
			}
		}
		else {
			state = STATE_ALL_OFF_STEADY;
			all_off();
			return;
		}
	}

	void analyze_state_brake_all_on_steady(Switches& switches) {
		// The hazard indicator supercedes everything
		if ( switches.hazard ) {
			state = STATE_HAZARD_ANIMATION;
			enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
			return;
		}

		// Before checking the brakes, check if this light needs
		// to change to a turn indicator.
		else if ( switches.left && side == SIDE_LEFT ) {
			state = STATE_TURN_ANIMATION;
			enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
			return;
		}
		else if ( switches.right && side == SIDE_RIGHT ) {
			state = STATE_TURN_ANIMATION;
			enable_animation( TH_ANIMATION_STEP_DELAY_MSEC );
			return;
		}

		if ( switches.brake ) {
			// no change
			return;
		}
		else {
			state = STATE_ALL_OFF_STEADY;
			all_off();
			return;
		}
	}

};

// Define the objects which model the 2 tail lights
TailLight left_tail_light( SIDE_LEFT,
	LEFT_LED0, LEFT_LED1, LEFT_LED2, LEFT_LED3 );

TailLight right_tail_light( SIDE_RIGHT,
	RIGHT_LED0, RIGHT_LED1, RIGHT_LED2, RIGHT_LED3 );

// The place where the switch info will be stored
Switches switches;

// When animating, we will sleep just a little bit before checking the state
// of the switches. This amount should be small, so we can quickly adapt to
// changes in the input.
constexpr int ANIMATION_SLEEP_DELAY = 10;

void setup() {
	if ( USE_SERIAL_PRINT ) {
		Serial.begin(9600);
	}

	// Set up the arduino pins
	left_tail_light.setup();
	right_tail_light.setup();

	pinMode(SWITCH_BRAKE, INPUT);
	pinMode(SWITCH_LEFT_TURN, INPUT);
	pinMode(SWITCH_RIGHT_TURN, INPUT);
	pinMode(SWITCH_HAZARD, INPUT);

	left_tail_light.all_off();
	right_tail_light.all_off();
}

void loop() {
	// Analyze the switches once, so that each tail light doesn't
	// have to do it.
	switches.evaluate();
	
	// Adjust the state machine and animate
	left_tail_light.loop( switches );
	right_tail_light.loop( switches );

	// Let the clock tick.
	delay(ANIMATION_SLEEP_DELAY);

	left_tail_light.tick( ANIMATION_SLEEP_DELAY );
	right_tail_light.tick( ANIMATION_SLEEP_DELAY );
}
