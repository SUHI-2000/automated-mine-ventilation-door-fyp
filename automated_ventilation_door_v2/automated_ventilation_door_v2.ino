/*
 * =============================================================================
 *   Automated Mine Ventilation Door - Control System (Version 2.0)
 * =============================================================================
 *   Author: S. Suheerman
 *   Project: Final Year Design Project - University of Moratuwa
 *   Date: [Enter Current Date]
 *
 *   Description:
 *   This code controls an automated overhead ventilation door for an underground
 *   mine. It is an enhanced version of an initial industrial training project.
 *
 *   Key Improvements from V1:
 *   - Non-Blocking State Machine: Uses millis() instead of delay() for all timing.
 *     This ensures the system is always responsive to sensor inputs, which is
 *     critical for safety.
 *   - Clear State Management: The door's logic is managed through defined states
 *     (CLOSED, OPENING, OPEN, CLOSING) for robust and predictable behavior.
 *   - Safety-First Logic: Obstacle detection is the highest priority and will
 *     interrupt any other operation to ensure the door opens safely.
 *   - Configurable Timings: All delays and thresholds are defined as constants
 *     at the top for easy tuning and maintenance.
 * =============================================================================
 */

// --- Pin Definitions ---
// Use 'const int' for pin numbers for type safety and to prevent accidental changes.
const int RADAR_SENSOR_PINS[] = {A0, A1, A5, A3, A4};
const int NUM_RADAR_SENSORS = sizeof(RADAR_SENSOR_PINS) / sizeof(RADAR_SENSOR_PINS[0]);

const int OBSTACLE_IR_PIN_1 = 7;
const int OBSTACLE_IR_PIN_2 = 8;

// Relay 1 controls the door motor (HIGH = Open/Run Motor, LOW = Close/Stop Motor)
const int DOOR_MOTOR_RELAY_PIN = 13;
// Relay 2 controls the status lights (HIGH = Green Light, LOW = Red Light)
const int STATUS_LIGHT_RELAY_PIN = 12;

const int BUZZER_PIN = 4;


// --- Configuration Constants ---
// Easily adjust the system's behavior by changing these values.
const long DOOR_TRAVEL_TIME_MS = 15000;      // 15 seconds: Time for the door to fully open or close.
const long DOOR_OPEN_DURATION_MS = 10000;    // 10 seconds: How long the door remains open after the last motion is detected.
const int MOTION_SENSITIVITY_THRESHOLD = 1;  // Min number of radar sensors that must be active to trigger opening (>= 1).


// --- State Management ---
// Using an enum makes the code much easier to read and understand.
enum DoorState {
  CLOSED,
  OPENING,
  OPEN,
  CLOSING
};

DoorState currentDoorState = CLOSED; // The door starts in the CLOSED state.

// --- Timing Variables ---
// These will store timestamps from millis() to manage non-blocking timing.
unsigned long stateChangeTimestamp = 0;
unsigned long lastMotionDetectedTimestamp = 0;


void setup() {
  Serial.begin(9600);
  Serial.println("Initializing Automated Ventilation Door System v2.0...");

  // Initialize Radar Sensor pins as inputs
  for (int i = 0; i < NUM_RADAR_SENSORS; i++) {
    pinMode(RADAR_SENSOR_PINS[i], INPUT);
  }

  // Initialize IR Obstacle Sensor pins as inputs
  pinMode(OBSTACLE_IR_PIN_1, INPUT);
  pinMode(OBSTACLE_IR_PIN_2, INPUT);

  // Initialize output pins
  pinMode(DOOR_MOTOR_RELAY_PIN, OUTPUT);
  pinMode(STATUS_LIGHT_RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Set initial state for outputs
  digitalWrite(DOOR_MOTOR_RELAY_PIN, LOW);   // Door is initially closed
  digitalWrite(STATUS_LIGHT_RELAY_PIN, LOW); // Red light is initially on
  digitalWrite(BUZZER_PIN, LOW);             // Buzzer is initially off

  Serial.println("System Initialized. Door is CLOSED.");
}


void loop() {
  // --- 1. Read All Sensor Inputs ---
  // This happens on every loop cycle, ensuring the system is always aware.

  // Check for obstacles (highest priority)
  bool obstacleDetected = (digitalRead(OBSTACLE_IR_PIN_1) == LOW || digitalRead(OBSTACLE_IR_PIN_2) == LOW);

  // Check for motion
  int activeRadarSensors = 0;
  for (int i = 0; i < NUM_RADAR_SENSORS; i++) {
    if (digitalRead(RADAR_SENSOR_PINS[i]) == HIGH) {
      activeRadarSensors++;
    }
  }
  bool motionDetected = (activeRadarSensors >= MOTION_SENSITIVITY_THRESHOLD);

  // If motion is detected, update the timestamp
  if (motionDetected) {
    lastMotionDetectedTimestamp = millis();
  }

  // --- 2. Safety Override Logic ---
  // This logic overrides the state machine if an obstacle is present.
  if (obstacleDetected) {
    // If an obstacle is detected, force the door to the OPENING state regardless of its current state.
    if (currentDoorState != OPENING && currentDoorState != OPEN) {
      currentDoorState = OPENING;
      stateChangeTimestamp = millis(); // Record the time we started opening
      Serial.println("! OBSTACLE DETECTED ! Forcing door to OPEN.");
    }
    digitalWrite(BUZZER_PIN, HIGH); // Sound the alarm continuously while obstacle is present
  } else {
    digitalWrite(BUZZER_PIN, LOW); // Turn off the buzzer if no obstacle is present
  }


  // --- 3. State Machine Logic ---
  // This switch statement manages the door's behavior based on its current state.
  // It only runs if the safety override for obstacles is not active.
  switch (currentDoorState) {

    case CLOSED:
      // If the door is closed and motion is detected, start opening it.
      if (motionDetected) {
        currentDoorState = OPENING;
        stateChangeTimestamp = millis(); // Start the opening timer
        Serial.println("Motion detected. State changed to OPENING.");
      }
      break;

    case OPENING:
      // If the door is opening, check if enough time has passed for it to be fully open.
      if (millis() - stateChangeTimestamp >= DOOR_TRAVEL_TIME_MS) {
        currentDoorState = OPEN;
        Serial.println("Door has finished opening. State changed to OPEN.");
      }
      break;

    case OPEN:
      // If the door is open, check how long it's been since the last motion was detected.
      // If the "open duration" has passed, start closing the door.
      if (millis() - lastMotionDetectedTimestamp >= DOOR_OPEN_DURATION_MS) {
        // We only start closing if there is NO obstacle. This check is an extra safeguard.
        if (!obstacleDetected) {
          currentDoorState = CLOSING;
          stateChangeTimestamp = millis(); // Start the closing timer
          Serial.println("Open time expired. State changed to CLOSING.");
        }
      }
      break;

    case CLOSING:
      // If the door is closing, check if enough time has passed for it to be fully closed.
      // Note: The obstacle override will interrupt this state if an obstacle appears.
      if (millis() - stateChangeTimestamp >= DOOR_TRAVEL_TIME_MS) {
        currentDoorState = CLOSED;
        Serial.println("Door has finished closing. State changed to CLOSED.");
      }
      break;
  }

  // --- 4. Set Physical Outputs Based on Current State ---
  // This section translates the current state into physical actions (relays).
  // This separation of logic and action makes the code cleaner.

  // Control the door motor
  if (currentDoorState == OPENING || currentDoorState == OPEN) {
    digitalWrite(DOOR_MOTOR_RELAY_PIN, HIGH); // Motor ON to keep door open or while opening
  } else {
    digitalWrite(DOOR_MOTOR_RELAY_PIN, LOW);  // Motor OFF when closed or closing
  }

  // Control the status lights
  if (currentDoorState == OPEN) {
    digitalWrite(STATUS_LIGHT_RELAY_PIN, HIGH); // Green light ON
  } else {
    digitalWrite(STATUS_LIGHT_RELAY_PIN, LOW);  // Red light ON (for CLOSED, OPENING, CLOSING)
  }
}