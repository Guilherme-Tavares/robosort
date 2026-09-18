#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define IR_PIN 2
#define SERVO_CHANNEL 8

// Calibração de pulso: ajustar conforme o servo
#define SERVO_MIN 150
#define SERVO_MAX 600

// Velocidade do movimento interpolado
#define STEP_DELAY 4   // ms entre subpassos; maior = mais lento
#define SUBSTEPS 1     // subpassos por grau

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

int currentAngle = 90;   // posição corrente do servo

void setServoAngle(uint8_t channel, int angle) {
  int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.setPWM(channel, 0, pulse);
}

// Interpolação smoothstep entre a posição corrente e o destino
void moveServoSmooth(uint8_t channel, int to) {
  int from = currentAngle;
  int distance = abs(to - from);

  if (distance == 0) return;

  int totalSteps = distance * SUBSTEPS;
  for (int i = 0; i <= totalSteps; i++) {
    float t = (float)i / totalSteps;
    float eased = t * t * (3.0f - 2.0f * t);
    setServoAngle(channel, from + (int)((to - from) * eased));
    delay(STEP_DELAY);
  }

  setServoAngle(channel, to);
  currentAngle = to;
}

void setup() {
  Serial.begin(115200);
  pinMode(IR_PIN, INPUT);

  pwm.begin();
  pwm.setPWMFreq(50);
  delay(10);

  setServoAngle(SERVO_CHANNEL, currentAngle);

  Serial.println("Digite um angulo (0-180). Sensor reportado a cada mudanca.");
}

int lastState = HIGH;

void loop() {
  int state = digitalRead(IR_PIN);
  if (state != lastState) {
    Serial.println(state == LOW ? "DET" : "livre");
    lastState = state;
  }

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      int a = line.toInt();
      if (a >= 0 && a <= 180) {
        moveServoSmooth(SERVO_CHANNEL, a);
        Serial.print("-> "); Serial.println(a);
      }
    }
  }
}