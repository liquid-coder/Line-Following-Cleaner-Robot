#include <Servo.h>

// Motor Driver Pin Definitions (for wheels)
#define IN1 5
#define IN2 4
#define ENA 6
#define IN3 3
#define IN4 2
#define ENB 9

// Motor Driver Pin Definitions (for vacuum and water pump)
#define VACUUM_RUN 7
#define WATER_RUN 8
#define VACUUM_WATER_ENA 10

// IR Sensor Pins
#define IR_LEFT A0
#define IR_RIGHT A1

// Motion Sensor Pin
#define MOTION_SENSOR A2
#define MOTION_LED 12 // LED to indicate motion detection

// Sonar Pins
#define TRIG_PIN A3
#define ECHO_PIN A4

// Brush Servo Pin (using the sonar servo for brush sweeping)
#define BRUSH_SERVO_PIN A5

// Distance threshold for obstacles (in cm)
#define OBSTACLE_THRESHOLD 20

// Motion Detection Debounce Time (in ms)
#define MOTION_CLEAR_TIME 1000

// Global Variables
Servo brushServo;          // Servo for brush (formerly sonar servo)
unsigned long sweepMillis = 0; // Timing for the brush servo
int sweepAngle = 0;         // Current angle of the brush servo
int sweepDirection = 1;     // Sweep direction: 1 for forward, -1 for backward
bool isSweeping = false;    // Whether the brush servo is sweeping

bool isObstacleDetected = false; // Obstacle detection flag
bool isMotionDetected = false;   // Motion detection flag
unsigned long motionClearMillis = 0; // Timer for motion to clear
unsigned long clearDuration = 1000;  // Time (in ms) for an obstacle to be considered cleared

// Function to measure distance using Sonar
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout at 30ms for faster reads
  float distance = (duration * 0.0343) / 2;
  return (distance >= 2 && distance <= 400) ? distance : -1; // Return -1 if out of range
}

// Functions to control the robot
void stopRobot() {
  // Stop wheels
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void moveForward() {
  // Move wheels forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 255); // Maximum speed
  analogWrite(ENB, 255);
}

void turnLeft() {
  // Turn wheels left
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 255);
  analogWrite(ENB, 0);
}

void turnRight() {
  // Turn wheels right
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 255);
}

void enableWaterVacuumBrush() {
  // Start vacuum, water pump, and brush
  digitalWrite(VACUUM_RUN, HIGH);
  digitalWrite(WATER_RUN, HIGH);
  analogWrite(VACUUM_WATER_ENA, 255);
  isSweeping = true; // Enable sweeping
}

void disableWaterVacuumBrush() {
  // Stop vacuum, water pump, and brush
  digitalWrite(VACUUM_RUN, LOW);
  digitalWrite(WATER_RUN, LOW);
  analogWrite(VACUUM_WATER_ENA, 0);
  isSweeping = false; // Disable sweeping
}

void setup() {
  // Wheel Motor Driver Pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Vacuum and Water Pump Motor Driver Pins
  pinMode(VACUUM_RUN, OUTPUT);
  pinMode(WATER_RUN, OUTPUT);
  pinMode(VACUUM_WATER_ENA, OUTPUT);

  // IR Sensors and Motion Detector
  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);

  // Sonar Pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Motion Sensor Pin
  pinMode(MOTION_SENSOR, INPUT);
  pinMode(MOTION_LED, OUTPUT);

  // Brush Servo (using sonar servo)
  brushServo.attach(BRUSH_SERVO_PIN);

  // Initialize Serial Monitor
  Serial.begin(9600);
  stopRobot(); // Start with the robot stopped
}

void loop() {
  // Check Sonar for obstacles
  float distance = measureDistance();
  static unsigned long clearStartMillis = 0; // Timer for continuous clear detection

  if (distance != -1 && distance <= OBSTACLE_THRESHOLD) {
    // If obstacle is detected, debounce and stop
    if (!isObstacleDetected) {
      Serial.println("Obstacle detected! Stopping.");
      digitalWrite(MOTION_LED, HIGH); // Turn on LED for obstacle
      isObstacleDetected = true;
      stopRobot(); // Stop robot immediately
      disableWaterVacuumBrush(); // Disable water, vacuum, and brush
    }
    clearStartMillis = 0; // Reset the clear detection timer
  } else {
    // If no obstacle is detected, check if obstacle is cleared for a while
    if (isObstacleDetected) {
      if (clearStartMillis == 0) {
        clearStartMillis = millis(); // Start timing how long it's clear
      } else if (millis() - clearStartMillis >= clearDuration) {
        Serial.println("Obstacle cleared! Resuming.");
        digitalWrite(MOTION_LED, LOW); // Turn off LED
        isObstacleDetected = false;
        clearStartMillis = 0; // Reset the clear detection timer
      }
    }
  }

  // Check Motion Sensor
  if (digitalRead(MOTION_SENSOR) == HIGH) {
    if (!isMotionDetected) {
      Serial.println("Motion detected! Stopping.");
      digitalWrite(MOTION_LED, HIGH); // Turn on LED for motion detection
      isMotionDetected = true;
      motionClearMillis = millis(); // Record the time motion was detected
      stopRobot(); // Stop robot
      disableWaterVacuumBrush(); // Disable water, vacuum, and brush
    }
  } else {
    if (isMotionDetected && millis() - motionClearMillis >= MOTION_CLEAR_TIME) {
      // Motion is cleared for the specified duration
      Serial.println("Motion cleared! Resuming.");
      digitalWrite(MOTION_LED, LOW); // Turn off LED
      isMotionDetected = false;
    }
  }

  // Line-following logic
  if (!isObstacleDetected && !isMotionDetected) {
    int leftIR = digitalRead(IR_LEFT);
    int rightIR = digitalRead(IR_RIGHT);

    if (leftIR == LOW || rightIR == LOW) {
      enableWaterVacuumBrush(); // Ensure water, vacuum, and brush are running
      if (leftIR == LOW && rightIR == LOW) {
        moveForward(); // Move forward if both sensors detect the line
      } else if (leftIR == LOW) {
        turnLeft(); // Turn left if only the left sensor detects the line
      } else if (rightIR == LOW) {
        turnRight(); // Turn right if only the right sensor detects the line
      }
    } else {
      // No IR sensor detects the line
      stopRobot();
      disableWaterVacuumBrush(); // Disable water, vacuum, and brush
    }
  }

  // Sweeping logic for brush servo
  if (isSweeping && millis() - sweepMillis >= 50) {
    sweepMillis = millis();
    sweepAngle += sweepDirection * 10;
    if (sweepAngle >= 180 || sweepAngle <= 0) {
      sweepDirection *= -1; // Reverse direction
    }
    brushServo.write(sweepAngle);
  }
}
