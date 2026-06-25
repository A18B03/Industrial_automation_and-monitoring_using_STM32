#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "stm32f4xx_hal.h"

// ====== Pin Definitions ======
#define LED_R PA0
#define LED_G PA1
#define LED_B PA2
#define BUTTON_PIN PA3

Adafruit_MPU6050 mpu;

// ====== I2S Handle ======
I2S_HandleTypeDef hi2s2;

unsigned long lastPrint = 0;
float soundLevel = 0.0;

// ======================================================
// 🕒 SYSTEM CLOCK CONFIGURATION
// ======================================================
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    while (1);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    while (1);

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
  PeriphClkInitStruct.PLLI2S.PLLI2SN = 192;
  PeriphClkInitStruct.PLLI2S.PLLI2SR = 2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    while (1);
}

// ======================================================
// 🎤 I2S2 INITIALIZATION (for INMP441 Mic)
// ======================================================
void I2S2_Init(void)
{
  __HAL_RCC_SPI2_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // WS = PB12, CK = PB13, SD = PB15
  GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_24B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_44K;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  hi2s2.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;

  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Serial.println("❌ I2S Init Failed!");
    while (1);
  }
}

// ======================================================
// 🔆 LED CONTROL (Digital Only for Now)
// ======================================================
void setLED(bool r, bool g, bool b)
{
  digitalWrite(LED_R, r ? HIGH : LOW);
  digitalWrite(LED_G, g ? HIGH : LOW);
  digitalWrite(LED_B, b ? HIGH : LOW);
}

void errorLED()
{
  for (int i = 0; i < 3; i++)
  {
    setLED(true, false, false); // Red blink
    delay(200);
    setLED(false, false, false);
    delay(200);
  }
  while (1);
}

// ======================================================
// 🚀 SETUP
// ======================================================
void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println("🔹 AIduino - MPU6050 + INMP441 Test Starting...");

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  HAL_Init();
  SystemClock_Config();
  I2S2_Init();

  Wire.begin();
  Wire.setClock(400000);  // Faster I2C for STM32

  // --- Initialize MPU6050 ---
  if (!mpu.begin())
  {
    Serial.println("❌ MPU6050 not found!");
    errorLED();
  }
  else
  {
    Serial.println("✅ MPU6050 ready.");
  }

  // --- Set safe ranges for correct scaling ---
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("✅ I2S (INMP441) initialized at 44.1kHz.");
  setLED(false, true, false); // Green = ready
  delay(1000);
  setLED(false, false, false);
}

// ======================================================
// 🔁 MAIN LOOP
// ======================================================
void loop()
{
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // --- Read Mic via I2S ---
  uint32_t sample = 0;
  HAL_StatusTypeDef res = HAL_I2S_Receive(&hi2s2, (uint16_t *)&sample, 1, 100);

  if (res == HAL_OK)
  {
    float normalized = ((int32_t)sample) / 8388607.0;
    soundLevel = fabs(normalized) * 10.0; // scale for LED use
  }
  else
  {
    Serial.println("I2S Read Error!");
  }

  // --- LED Indication Logic ---
  if (temp.temperature > 40.0)
  {
    setLED(true, false, false); // 🔴 Red if temperature > 40°C
  }
  else if (soundLevel > 0.0)
  {
    setLED(false, false, true); // Blue = loud sound
  }
  else if (fabs(a.acceleration.x) > 1.0 || fabs(a.acceleration.y) > 1.0)
  {
    setLED(true, false, false); // Red = strong motion
  }
  else
  {
    setLED(false, true, false); // Green = normal
  }

  // --- Serial Monitor Output ---
  if (millis() - lastPrint >= 1000)
  {
    lastPrint = millis();

    Serial.print("Accel: ");
    Serial.print(a.acceleration.x, 2); Serial.print(", ");
    Serial.print(a.acceleration.y, 2); Serial.print(", ");
    Serial.print(a.acceleration.z, 2);

    Serial.print(" | Gyro: ");
    Serial.print(g.gyro.x, 2); Serial.print(", ");
    Serial.print(g.gyro.y, 2); Serial.print(", ");
    Serial.print(g.gyro.z, 2);

    Serial.print(" | Temp: ");
    Serial.print(temp.temperature, 2);
    Serial.print(" °C | Sound Level: ");
    Serial.println(soundLevel, 2);
  }

  delay(100);
}
