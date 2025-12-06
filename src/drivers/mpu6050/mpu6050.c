#include "./mpu6050.h"
#include "pico/stdlib.h"

#define MPU6500_ADDR        0x68
#define MPU6500_PWR_MGMT    0x6B
#define MPU6500_ACCEL_X     0x3B
#define MPU6500_INT_ENABLE  0x38     // habilita interrupções
#define MPU6500_INT_PIN_CFG 0x37     

void mpu6500_init(i2c_inst_t* i2c) 
{
    uint8_t buf[2];

    // Wake up
    buf[0] = MPU6500_PWR_MGMT;
    buf[1] = 0x00;
    i2c_write_blocking(i2c, MPU6500_ADDR, buf, 2, false);

    // Configura pino de interrupção como active-high, push-pull
    buf[0] = MPU6500_INT_PIN_CFG;
    buf[1] = 0x00;
    i2c_write_blocking(i2c, MPU6500_ADDR, buf, 2, false);

    // Ativa interrupção de DATA READY
    buf[0] = MPU6500_INT_ENABLE;
    buf[1] = 0x01; 
    i2c_write_blocking(i2c, MPU6500_ADDR, buf, 2, false);
}

void mpu6500_read_raw(i2c_inst_t* i2c, mpu6500_data_t* data) 
{
    uint8_t buffer[14];
    uint8_t reg = MPU6500_ACCEL_X;

    i2c_write_blocking(i2c, MPU6500_ADDR, &reg, 1, true);
    i2c_read_blocking(i2c, MPU6500_ADDR, buffer, 14, false);

    data->accel[0] = (buffer[0] << 8) | buffer[1];
    data->accel[1] = (buffer[2] << 8) | buffer[3];
    data->accel[2] = (buffer[4] << 8) | buffer[5];
    data->temp     = (buffer[6] << 8) | buffer[7];
    data->gyro[0]  = (buffer[8] << 8) | buffer[9];
    data->gyro[1]  = (buffer[10] << 8) | buffer[11];
    data->gyro[2]  = (buffer[12] << 8) | buffer[13];
}
