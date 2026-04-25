---
name: Motor Control Parameters
description: TB6612/TB6600, 28BYJ-48 stepper motor, and grayscale sensor parameters
type: reference
---

## TB6612 Dual Motor Driver (Primary DC Motor Driver)

**Datasheet**: TB6612FNG Datasheet.pdf, 塔克创新 l TB6612双路编码器电机驱动用户手册 V1.0.pdf

### Key Specifications
- **Max Output Current**: 1.2A per channel (recommended continuous: 1A)
- **Peak Current**: Up to 2A short-term
- **Motor Voltage Range**: 4.5V - 13.5V (independent from logic)
- **Logic Voltage**: 3.3V/5V compatible
- **Operating Temperature**: -20°C to +85°C
- **Thermal Shutdown**: Auto-disable if TJ > 150°C

### Input Pin Functions
```
TB6612 Inputs          Function
─────────────────────────────────────────
IN1, IN2              Motor A direction control
IN3, IN4              Motor B direction control
ENA, ENB              PWM speed control (or GPIO for fixed speed)
STANDBY               Global enable/disable (active HIGH)
GND, VM               Power connections
```

### Motor Control Logic (IN1/IN2 and IN3/IN4)
```
IN1  IN2   Action
─────────────────────
0    0     STOP (free-run)
0    1     Forward
1    0     Reverse
1    1     STOP (brake)
```

### PWM Configuration Recommendations
- **PWM Frequency**: 10-20 kHz recommended (avoid <1 kHz for smoothness)
- **PWM Resolution**: 10-bit (0-1023) gives sufficient granularity
- **Dead Time**: Not required for TB6612 (handles internally)
- **Starting Voltage**: Recommend PWM > 30% to avoid stall

**Why 10 kHz?** 
- High enough to avoid audible noise (<20 kHz human hearing limit)
- Low enough that STM32F407 @ 168 MHz can handle easily
- Good balance for motor response

### Electrical Characteristics
| Parameter | Min | Typ | Max | Unit |
|-----------|-----|-----|-----|------|
| Supply Voltage (VM) | 4.5 | - | 13.5 | V |
| Logic High (VIH) | 2.0 | - | - | V |
| Logic Low (VIL) | - | - | 0.8 | V |
| Output Current (per channel) | - | - | 1.2 | A |
| Thermal Resistance | - | - | 60 | °C/W |

### Overcurrent Protection
- Internal current limit: ~2A
- Auto shutdown at 150°C junction temperature
- Recovery: Automatic after ~1ms when cooled

### STM32 Integration Example
```c
// Pins (configure in STM32CubeMX)
#define TB6612_IN1   GPIO_PIN_X   // PA_
#define TB6612_IN2   GPIO_PIN_Y   // PA_
#define TB6612_IN3   GPIO_PIN_Z   // PA_
#define TB6612_IN4   GPIO_PIN_W   // PA_
#define TB6612_ENA   TIM_CHANNEL_1  // PWM from TIM2
#define TB6612_ENB   TIM_CHANNEL_2  // PWM from TIM2
#define TB6612_STANDBY GPIO_PIN_S  // Always HIGH (optional)

// Enable TB6612
HAL_GPIO_WritePin(GPIOA, TB6612_STANDBY, GPIO_PIN_SET);

// Start PWM on ENA/ENB
HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

// Set motor A to forward at 80% speed
HAL_GPIO_WritePin(GPIOA, TB6612_IN1, GPIO_PIN_SET);
HAL_GPIO_WritePin(GPIOA, TB6612_IN2, GPIO_PIN_RESET);
__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 820);  // 80% of 1023
```

---

## ULN2003 Stepper Driver (28BYJ-48)

**Datasheets**: ULN2003英文数据手册.pdf, ULN2003中文数据手册.pdf, 28BYJ48规格书.doc

### ULN2003 Specifications
- **Channels**: 4 independent outputs
- **Max Output Current**: 500mA per channel (recommended: 200-300mA continuous)
- **Input Logic**: 3.3V/5V compatible
- **Freewheeling Diodes**: Built-in (important for inductive loads)
- **Thermal Shutdown**: Auto-disable at ~150°C

### 28BYJ-48 Stepper Motor Specifications
- **Voltage**: 5V DC nominal
- **Phase Count**: 4-phase unipolar
- **Step Angle**: 5.625° (single phase)
- **Gear Reduction**: Internal 1:64 gear
- **Effective Step Angle**: 5.625° / 64 = 0.0879° (actually 64 steps per revolution without gearing, then 1:64 reduction)
- **Total Steps per Revolution**: 2048 (with gearing)
- **Torque**: ~30 g·cm (without load, typical)
- **Max Current**: 200mA per coil
- **Winding Resistance**: ~50Ω per coil

### Control Principle: 4-Phase Sequence

**Full Step Mode** (standard):
```
Step  IN1  IN2  IN3  IN4   Comment
───────────────────────────────────
0     1    0    0    0     Phase A on
1     1    1    0    0     Phases A+B on
2     0    1    0    0     Phase B on
3     0    1    1    0     Phases B+C on
4     0    0    1    0     Phase C on
5     0    0    1    1     Phases C+D on
6     0    0    0    1     Phase D on
7     1    0    0    1     Phases D+A on
```

**Wave Mode** (less torque, less current):
```
Step  IN1  IN2  IN3  IN4
───────────────────────
0     1    0    0    0
1     0    1    0    0
2     0    0    1    0
3     0    0    0    1
```

### Timing and Speed

- **Stepping Frequency**: 100-1000 Hz typical (controllable)
- **Speed (RPM)** = (Steps per second × 60) / 2048
  - Example: 500 Hz → (500 × 60) / 2048 = 14.6 RPM
- **Acceleration Time**: Typically ramp over 500-1000ms to avoid stalling
- **Holding Torque**: Maintain with last coil energized

### STM32 Integration Example
```c
#define STEPPER_IN1 GPIO_PIN_8   // PA8
#define STEPPER_IN2 GPIO_PIN_9   // PA9
#define STEPPER_IN3 GPIO_PIN_10  // PA10
#define STEPPER_IN4 GPIO_PIN_11  // PA11

// Full-step sequence table
const uint8_t stepper_step[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

// Single step with delay
void stepper_step(uint8_t direction, uint16_t delay_ms) {
    static uint8_t step_index = 0;
    
    // Direction: 1 = forward, 0 = reverse
    if (direction) {
        step_index = (step_index + 1) % 8;
    } else {
        step_index = (step_index + 7) % 8;
    }
    
    // Set coils
    HAL_GPIO_WritePin(GPIOA, STEPPER_IN1, (stepper_step[step_index][0]) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, STEPPER_IN2, (stepper_step[step_index][1]) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, STEPPER_IN3, (stepper_step[step_index][2]) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, STEPPER_IN4, (stepper_step[step_index][3]) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    
    HAL_Delay(delay_ms);
}

// Rotate N steps
void stepper_rotate(int steps, uint16_t step_delay_ms) {
    int direction = (steps > 0) ? 1 : 0;
    int count = steps > 0 ? steps : -steps;
    
    for (int i = 0; i < count; i++) {
        stepper_step(direction, step_delay_ms);
    }
}
```

### Troubleshooting
- **Motor doesn't turn**: Check coil sequence order (IN1-IN4 pins must match sequence)
- **Weak torque**: Increase step delay (slower = more torque), or use wave mode first then switch to full-step
- **Heating**: Normal up to 60°C; if >80°C, reduce current or increase cooling
- **Missing steps**: Increase step delay (too fast causes stalling)

---

## 8-Channel Grayscale Sensor (Line Tracking)

**Datasheets**: 亚博智能灰度循迹模块用户入门手册.pdf, 循迹模块数据读取.pdf

### Sensor Specifications
- **Sensor Count**: 8 IR channels in linear array
- **Sensor Type**: Reflective IR (TCRT5000 or similar)
- **Output Mode**: Analog (0-1023 ADC) or Digital (GPIO high/low)
- **Supply Voltage**: 5V (typical), some 3.3V variants
- **Operating Distance**: 1-3 cm from surface
- **Surface Colors**: Optimized for black/white distinction

### Output Interpretation

**Analog Mode**:
- Black surface (reflective) → Lower ADC value (~0-300)
- White surface (non-reflective) → Higher ADC value (~700-1023)
- Threshold typically around 500-600 (depends on lighting)

**Digital Mode**:
- Black → GPIO LOW (0)
- White → GPIO HIGH (1)
- Digital output already thresholded

### Typical Calibration Procedure

1. **Black calibration**: Hold sensor 2 cm above black paper, measure ADC value (e.g., 150)
2. **White calibration**: Hold sensor 2 cm above white paper, measure ADC value (e.g., 900)
3. **Threshold**: (Black + White) / 2 = (150 + 900) / 2 = 525
4. **Adjustment**: Use potentiometer on module to fine-tune threshold if available

### STM32 Integration (ADC Mode)

**Configuration** (STM32CubeMX):
- Enable ADC1 (or ADC2/3)
- Setup 8 channels: PA0-PA7 (or your pins)
- Trigger: Continuous scan or DMA
- DMA: Enable for continuous transfer to RAM buffer

```c
#define GRAYSCALE_CHANNELS 8
uint16_t grayscale_adc[GRAYSCALE_CHANNELS];

void grayscale_init(void) {
    // Assuming ADC1 with DMA2 Stream 0
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)grayscale_adc, GRAYSCALE_CHANNELS);
}

void grayscale_read(uint8_t binary[8]) {
    for (int i = 0; i < 8; i++) {
        binary[i] = (grayscale_adc[i] > 500) ? 1 : 0;  // Threshold
    }
}

// Calculate line position (-35 to +35, center = 0)
int grayscale_get_position(const uint8_t binary[8]) {
    int sum = 0, count = 0;
    for (int i = 0; i < 8; i++) {
        if (binary[i] == 1) {  // Line detected
            sum += (i - 3) * 10;  // Weight: -30 to +30
            count++;
        }
    }
    return count > 0 ? sum / count : 0;
}
```

### Typical Line-Following Algorithm

```c
int position = grayscale_get_position(binary);
int target_speed = 600;  // Base speed

if (position < -10) {
    // Line on left, turn left
    left_motor_speed = target_speed * 0.5;
    right_motor_speed = target_speed;
} else if (position > 10) {
    // Line on right, turn right
    left_motor_speed = target_speed;
    right_motor_speed = target_speed * 0.5;
} else {
    // Centered, go straight
    left_motor_speed = target_speed;
    right_motor_speed = target_speed;
}
```

### Troubleshooting
- **All sensors read same value**: Check power supply (should be 5V stable)
- **No variation between black/white**: Sensor too far or too close (adjust to 1-3 cm)
- **Noisy readings**: 
  - Clean sensor lens (dust blocks IR)
  - Add capacitive filter (RC low-pass, ~10 kHz cutoff)
  - Increase ADC sample time in STM32CubeMX
- **One sensor not responding**: Check PCB solder joints; may be defective

---

## Quick Parameter Summary

| Component | Parameter | Value | Notes |
|-----------|-----------|-------|-------|
| TB6612 | PWM Freq | 10-20 kHz | Avoid <1 kHz |
| TB6612 | Max Current | 1.2A/ch | Thermal limit ~2A peak |
| TB6612 | Motor Voltage | 4.5-13.5V | Independent from 3.3V logic |
| 28BYJ-48 | Supply | 5V | NOT 3.3V |
| 28BYJ-48 | Step Angle | 5.625° | With gearing, 2048 steps/rev |
| 28BYJ-48 | Step Delay | 2-10ms | Shorter = faster, more torque |
| Grayscale | Threshold | 500-600 | Depends on surface reflectance |
| Grayscale | Distance | 1-3 cm | Optimal detection range |

**Key Documentation Files**:
- Motor driver: TB6612FNG Datasheet.pdf
- Stepper motor: 28BYJ48规格书.doc, ULN2003英文数据手册.pdf
- Line sensor: 亚博智能灰度循迹模块用户入门手册.pdf
