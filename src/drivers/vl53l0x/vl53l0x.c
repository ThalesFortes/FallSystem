#include "vl53l0x.h"
#include "pico/stdlib.h"

// Registradores principais (mapa usado por bibliotecas Pololu/Adafruit)
#define REG_SYSRANGE_START              0x00
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO 0x0A
#define REG_SYSTEM_INTERRUPT_CLEAR      0x0B
#define REG_SYSTEM_SEQUENCE_CONFIG      0x01
#define REG_GPIO_HV_MUX_ACTIVE_HIGH     0x84
#define REG_RESULT_INTERRUPT_STATUS     0x13
#define REG_RESULT_RANGE_STATUS         0x14
#define REG_MSRC_CONFIG_CONTROL         0x60
#define REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT 0x44
#define REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0 0xB0
#define REG_DYNAMIC_SPAD_REF_EN_START_OFFSET 0x4F
#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD 0x4E
#define REG_GLOBAL_CONFIG_REF_EN_START_SELECT 0xB6
#define REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV 0x89
#define REG_I2C_SLAVE_DEVICE_ADDRESS    0x8A

// Bancos "ocultos" de configuração
#define REG_IDENTIFICATION_MODEL_ID     0xC0
#define REG_SOFT_RESET_GO2_SOFT_RESET_N 0xBF

// Sequência de acesso estendida (bancos 0xFF/0x80/0x00/0x91)
#define REG_BANK_SELECT_FF              0xFF
#define REG_BANK_SELECT_80              0x80
#define REG_STOP_VARIABLE               0x91
#define REG_BANK_SELECT_00              0x00

static inline bool i2c_write8(i2c_inst_t *i2c, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_write_blocking(i2c, VL53L0X_ADDR, buf, 2, false) == 2;
}

static inline bool i2c_read8(i2c_inst_t *i2c, uint8_t reg, uint8_t *val) {
    if (i2c_write_blocking(i2c, VL53L0X_ADDR, &reg, 1, true) != 1) return false;
    return i2c_read_blocking(i2c, VL53L0X_ADDR, val, 1, false) == 1;
}

static inline bool i2c_read16(i2c_inst_t *i2c, uint8_t reg, uint16_t *val) {
    uint8_t tmp[2];
    if (i2c_write_blocking(i2c, VL53L0X_ADDR, &reg, 1, true) != 1) return false;
    if (i2c_read_blocking(i2c, VL53L0X_ADDR, tmp, 2, false) != 2) return false;
    *val = ((uint16_t)tmp[0] << 8) | tmp[1];
    return true;
}

static uint8_t stop_variable = 0;

bool vl53l0x_init(i2c_inst_t *i2c) {
    // Checagem simples de ID do chip
    uint8_t id = 0;
    if (!i2c_read8(i2c, REG_IDENTIFICATION_MODEL_ID, &id)) return false;
    // VL53L0X costuma retornar 0xEE no C0
    if (id != 0xEE) {
    }

    // Habilita pull-up alto para SCL/SDA (conforme seq. comum)
    i2c_write8(i2c, REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, 0x01);

    // "Acesso estendido" para ler stop_variable
    i2c_write8(i2c, 0x88, 0x00);
    i2c_write8(i2c, REG_BANK_SELECT_80, 0x01);
    i2c_write8(i2c, REG_BANK_SELECT_FF, 0x01);
    i2c_write8(i2c, REG_BANK_SELECT_00, 0x00);
    i2c_read8(i2c, REG_STOP_VARIABLE, &stop_variable);
    i2c_write8(i2c, REG_BANK_SELECT_00, 0x01);
    i2c_write8(i2c, REG_BANK_SELECT_FF, 0x00);
    i2c_write8(i2c, REG_BANK_SELECT_80, 0x00);

    // Desabilita alguns limit checks MSRC/PRE_RANGE
    uint8_t msrc = 0;
    i2c_read8(i2c, REG_MSRC_CONFIG_CONTROL, &msrc);
    i2c_write8(i2c, REG_MSRC_CONFIG_CONTROL, msrc | 0x12);

    // Limite de taxa de sinal final (0.25 MCPS -> 0x0019 em Q9.7)
    i2c_write8(i2c, REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, 0x00);
    i2c_write8(i2c, REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT + 1, 0x19);

    // Configura SPADs de referência (versão mínima)
    i2c_write8(i2c, REG_BANK_SELECT_FF, 0x01);
    i2c_write8(i2c, REG_DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
    i2c_write8(i2c, REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);
    i2c_write8(i2c, REG_BANK_SELECT_FF, 0x00);
    i2c_write8(i2c, REG_GLOBAL_CONFIG_REF_EN_START_SELECT, 0xB4);

    // Interrupção: sinalizar "new sample ready"
    i2c_write8(i2c, REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
    uint8_t gpio = 0;
    i2c_read8(i2c, REG_GPIO_HV_MUX_ACTIVE_HIGH, &gpio);
    i2c_write8(i2c, REG_GPIO_HV_MUX_ACTIVE_HIGH, gpio & ~0x10);
    i2c_write8(i2c, REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    // Habilita todas etapas de sequência
    i2c_write8(i2c, REG_SYSTEM_SEQUENCE_CONFIG, 0xFF);

    // Pequena calibração única (ref. simplificada)
    i2c_write8(i2c, REG_SYSRANGE_START, 0x01);
    // espera primeiro resultado e limpa interrupção
    absolute_time_t t0 = get_absolute_time();
    while (true) {
        uint8_t st = 0;
        i2c_read8(i2c, REG_RESULT_INTERRUPT_STATUS, &st);
        if (st & 0x07) break;
        if (absolute_time_diff_us(t0, get_absolute_time()) > 200000) break; 
        tight_loop_contents();
    }
    i2c_write8(i2c, REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    i2c_write8(i2c, REG_SYSRANGE_START, 0x00);

    return true;
}

bool vl53l0x_start_continuous(i2c_inst_t *i2c) {
    // Sequência para continuous mode
    i2c_write8(i2c, REG_BANK_SELECT_80, 0x01);
    i2c_write8(i2c, REG_BANK_SELECT_FF, 0x01);
    i2c_write8(i2c, REG_BANK_SELECT_00, 0x00);
    i2c_write8(i2c, REG_STOP_VARIABLE, stop_variable);
    i2c_write8(i2c, REG_BANK_SELECT_00, 0x01);
    i2c_write8(i2c, REG_BANK_SELECT_FF, 0x00);
    i2c_write8(i2c, REG_BANK_SELECT_80, 0x00);

    // Inicia medição contínua
    if (!i2c_write8(i2c, REG_SYSRANGE_START, 0x02)) return false;

    // Espera bit de start apagar
    absolute_time_t t0 = get_absolute_time();
    while (true) {
        uint8_t v = 0;
        i2c_read8(i2c, REG_SYSRANGE_START, &v);
        if ((v & 0x01) == 0) break;
        if (absolute_time_diff_us(t0, get_absolute_time()) > 200000) break;
        tight_loop_contents();
    }
    return true;
}

bool vl53l0x_stop_continuous(i2c_inst_t *i2c) {
    return i2c_write8(i2c, REG_SYSRANGE_START, 0x01); // coloca em single-shot idle
}

bool vl53l0x_read_range_mm(i2c_inst_t *i2c, uint16_t *range_mm, uint32_t timeout_ms) {
    absolute_time_t t0 = get_absolute_time();

    // Espera dado pronto (RESULT_INTERRUPT_STATUS bits[2:0] != 0)
    while (true) {
        uint8_t st = 0;
        if (!i2c_read8(i2c, REG_RESULT_INTERRUPT_STATUS, &st)) return false;
        if (st & 0x07) break;
        if ((uint32_t)(absolute_time_diff_us(t0, get_absolute_time())/1000) > timeout_ms) return false;
        sleep_ms(1);
    }

    // range_mm está em RESULT_RANGE_STATUS + 10 (16 bits, big-endian)
    if (!i2c_read16(i2c, REG_RESULT_RANGE_STATUS + 10, range_mm)) return false;

    // Limpa interrupção de "new sample ready"
    if (!i2c_write8(i2c, REG_SYSTEM_INTERRUPT_CLEAR, 0x01)) return false;

    return true;
}