/**
 * @file      4AI-16-&-24BIT-v1.0
 * @brief     Raw Acquisition Firmware for Dual ADC AI PCB
 * @details   Outputs normalized raw digital steps for all 4 channels.
 * 16-bit is mathematically normalized to a 0-5V FSR.
 * 24-bit natively utilizes the 5V power supply as its reference.
 * Optional 2x scaling blocks are included (commented out) to stretch 
 * the 15-bit/23-bit single-ended hardware limits into full 16/24-bit ranges.
 */

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_ADS1X15.h>
#include <ADS1220_WE.h>

#define ADS1220_CS_PIN    10
#define ADS1220_DRDY_PIN  -1  

Adafruit_ADS1115 ads1115;
ADS1220_WE ads1220 = ADS1220_WE(ADS1220_CS_PIN, ADS1220_DRDY_PIN);

// =====================================================
// DATA NORMALIZATION & SCALING FUNCTIONS
// =====================================================

/**
 * @brief Stretches the ADS1115 raw data to behave like a 5V ADC
 * @details Multiplies the raw 6.144V scale data by 1.2288 (6.144 / 5.0)
 */
int16_t normalize16BitTo5V(int16_t raw_data)
{
    if (raw_data < 0) return 0; 
    float scaled = raw_data * 1.2288;
    if (scaled > 32767.0) return 32767; 
    return (int16_t)scaled;
}

/**
 * @brief Stretches 15-bit data (0-32767) to full 16-bit scale (0-65535).
 */
uint16_t scale32kTo65k(int16_t val_32k)
{
    if (val_32k < 0) return 0; 
    return (uint16_t)(val_32k * 2);
}

/**
 * @brief Stretches 23-bit data (0-8,388,607) to full 24-bit scale (0-16,777,215).
 */
uint32_t scale8mTo16m(int32_t val_8m)
{
    if (val_8m < 0) return 0; 
    return (uint32_t)(val_8m * 2);
}

// =====================================================
// INITIALIZATION
// =====================================================
void setup()
{
    Serial.begin(115200);
    while (!Serial);

    Serial.println("\n--- UNIVERSAL AI MODULE BOOTING ---");
    Serial.println("Output: Normalized Raw Hardware Resolution");

    // 1. Initialize 16-Bit ADS1115
    Wire.begin();
    if (!ads1115.begin(0x48)) {
        Serial.println("FAIL: ADS1115 (I2C) not responding!");
        while (1);
    }
    ads1115.setGain(GAIN_TWOTHIRDS); 
    ads1115.setDataRate(RATE_ADS1115_8SPS); 
    Serial.println("-> ADS1115 (16-bit) OK");

    // 2. Initialize 24-Bit ADS1220
    SPI.begin();
    if (!ads1220.init()) {
        Serial.println("FAIL: ADS1220 (SPI) not responding!");
        while (1);
    }
    ads1220.bypassPGA(true);                   
    ads1220.setGain(ADS1220_GAIN_1);           
    ads1220.setDataRate(ADS1220_DR_LVL_1);     
    ads1220.setConversionMode(ADS1220_CONTINUOUS);
    ads1220.setAvddAvssAsVrefAndCalibrate(); // Locks internal reference to Arduino 5V
    Serial.println("-> ADS1220 (24-bit) OK");

    Serial.println("--- ACQUISITION STARTED ---\n");
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop()
{
    // --- 1. Fetch & Normalize 16-Bit Data ---
    int16_t raw_16_ch0 = normalize16BitTo5V(ads1115.readADC_SingleEnded(0));
    int16_t raw_16_ch1 = normalize16BitTo5V(ads1115.readADC_SingleEnded(1));
    int16_t raw_16_ch2 = normalize16BitTo5V(ads1115.readADC_SingleEnded(2));
    int16_t raw_16_ch3 = normalize16BitTo5V(ads1115.readADC_SingleEnded(3));

    uint16_t out_16_ch0 = raw_16_ch0;
    uint16_t out_16_ch1 = raw_16_ch1;
    uint16_t out_16_ch2 = raw_16_ch2;
    uint16_t out_16_ch3 = raw_16_ch3;

    // --- OPTIONAL: 16-BIT 2x SCALING ---
    // Uncomment to stretch 15-bit single-ended data to fill full 16-bit range (0 to ~65534)
    // --------------------------------------------------------------------------------------
    // out_16_ch0 = scale32kTo65k(out_16_ch0);
    // out_16_ch1 = scale32kTo65k(out_16_ch1);
    // out_16_ch2 = scale32kTo65k(out_16_ch2);
    // out_16_ch3 = scale32kTo65k(out_16_ch3);

    // --- 2. Fetch 24-Bit Data ---
    ads1220.setCompareChannels(ADS1220_MUX_0_AVSS); delay(50);
    int32_t raw_24_ch0 = ads1220.getRawData();

    ads1220.setCompareChannels(ADS1220_MUX_1_AVSS); delay(50);
    int32_t raw_24_ch1 = ads1220.getRawData();

    ads1220.setCompareChannels(ADS1220_MUX_2_AVSS); delay(50);
    int32_t raw_24_ch2 = ads1220.getRawData();

    ads1220.setCompareChannels(ADS1220_MUX_3_AVSS); delay(50);
    int32_t raw_24_ch3 = ads1220.getRawData();

    // Filter negative noise and assign to unsigned 32-bit for safe scaling
    uint32_t out_24_ch0 = (raw_24_ch0 > 0) ? raw_24_ch0 : 0;
    uint32_t out_24_ch1 = (raw_24_ch1 > 0) ? raw_24_ch1 : 0;
    uint32_t out_24_ch2 = (raw_24_ch2 > 0) ? raw_24_ch2 : 0;
    uint32_t out_24_ch3 = (raw_24_ch3 > 0) ? raw_24_ch3 : 0;

    // --- OPTIONAL: 24-BIT 2x SCALING ---
    // Uncomment to stretch 23-bit single-ended data to fill full 24-bit range (0 to ~16,777,214)
    // --------------------------------------------------------------------------------------
    // out_24_ch0 = scale8mTo16m(out_24_ch0);
    // out_24_ch1 = scale8mTo16m(out_24_ch1);
    // out_24_ch2 = scale8mTo16m(out_24_ch2);
    // out_24_ch3 = scale8mTo16m(out_24_ch3);

    // --- 3. Print to Serial Monitor ---
    Serial.print("[16-BIT | Raw] ");
    Serial.print("CH0: "); Serial.print(out_16_ch0);
    Serial.print(" | CH1: "); Serial.print(out_16_ch1);
    Serial.print(" | CH2: "); Serial.print(out_16_ch2);
    Serial.print(" | CH3: "); Serial.print(out_16_ch3);

    Serial.print("   ||   ");

    Serial.print("[24-BIT | Raw] ");
    Serial.print("CH0: "); Serial.print(out_24_ch0);
    Serial.print(" | CH1: "); Serial.print(out_24_ch1);
    Serial.print(" | CH2: "); Serial.print(out_24_ch2);
    Serial.print(" | CH3: "); Serial.print(out_24_ch3);

    Serial.println();
}