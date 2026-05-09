// ===== CONFIG =====
#define NUM_SENSORS 3

const uint8_t trigPins[NUM_SENSORS] = { 2, 3, 4 };
const uint8_t ledPins[NUM_SENSORS] = { 10, 11, 12 };

// ===== RANGE LIMIT =====
#define MAX_DISTANCE_CM 7  // ~3 inches

// ===== STATE =====
volatile uint16_t startTime = 0;
volatile uint16_t endTime = 0;

volatile bool risingCaptured = false;

volatile uint8_t currentSensor = 0;

// Store raw pulse widths
volatile uint16_t pulseWidths[NUM_SENSORS];

// ===== LED STATE =====
enum LedState {
  LED_OFF,
  LED_ON,
  LED_PULSE
};

volatile LedState ledState[NUM_SENSORS] = {
  LED_OFF, LED_OFF, LED_OFF
};

// ===== TIMING =====
volatile uint16_t tickCount = 0;
const uint16_t SENSOR_PERIOD = 30;  // faster now (short range)

// ===== TIMER1 =====
void setupTimer1() {
  TCCR1A = 0;
  TCCR1B = 0;

  TCCR1B |= (1 << CS11);
  TCCR1B |= (1 << ICES1);

  TIMSK1 |= (1 << ICIE1);
}

// ===== TIMER2 =====
void setupTimer2() {
  TCCR2A = 0;
  TCCR2B = 0;

  TCCR2A |= (1 << WGM21);
  TCCR2B |= (1 << CS22);

  OCR2A = 249;

  TIMSK2 |= (1 << OCIE2A);
}

// ===== TRIGGER =====
inline void triggerSensor(uint8_t pin) {
  digitalWrite(pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(pin, LOW);
}

// ===== INPUT CAPTURE ISR =====
ISR(TIMER1_CAPT_vect) {

  if (!risingCaptured) {
    startTime = ICR1;
    risingCaptured = true;
    TCCR1B &= ~(1 << ICES1);
  } else {
    endTime = ICR1;

    uint16_t width = endTime - startTime;
    pulseWidths[currentSensor] = width;

    risingCaptured = false;

    // Convert to distance (cm)
    float distance = width * 0.0343 / 2.0;

    // ===== RANGE FILTER =====
    if (distance <= MAX_DISTANCE_CM && distance > 0) {
      ledState[currentSensor] = LED_PULSE;  // valid detection
    } else {
      ledState[currentSensor] = LED_OFF;  // ignore far objects
    }

    TCCR1B |= (1 << ICES1);
  }
}

// ===== SCHEDULER ISR =====
ISR(TIMER2_COMPA_vect) {

  tickCount++;

  static uint16_t lastTrigger = 0;

  if ((tickCount - lastTrigger) >= SENSOR_PERIOD) {

    if (!risingCaptured) {

      ledState[currentSensor] = LED_ON;

      triggerSensor(trigPins[currentSensor]);

      lastTrigger = tickCount;

      currentSensor = (currentSensor + 1) % NUM_SENSORS;
    }
  }
}

// ===== SETUP =====
void setup() {

  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(trigPins[i], OUTPUT);
    digitalWrite(trigPins[i], LOW);

    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  pinMode(8, INPUT);

  setupTimer1();
  setupTimer2();

  sei();
}

// ===== LOOP =====
void loop() {

  for (int i = 0; i < NUM_SENSORS; i++) {

    switch (ledState[i]) {

      case LED_OFF:
        digitalWrite(ledPins[i], LOW);
        break;

      case LED_ON:
        digitalWrite(ledPins[i], HIGH);
        break;

      case LED_PULSE:
        digitalWrite(ledPins[i], LOW);
        delay(20);
        ledState[i] = LED_OFF;
        break;
    }
  }
}