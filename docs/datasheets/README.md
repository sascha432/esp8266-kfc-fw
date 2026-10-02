# Datasheets

Vendor documents for parts that are implemented by hand in this firmware.

## MPU-6000 / MPU-6050

`Sensor_MPU6050` (`src/plugins/sensor/`) talks to the chip directly over I2C, so the register map is the
reference for it.

- `MPU-6000-MPU-6050-datasheet.pdf` - "MPU-6000 and MPU-6050 Product Specification" (Revision 3.4,
  52 pages, downloaded from `https://www.cdiweb.com/datasheets/invensense/mpu-6050_datasheet_v3%204.pdf`)
- `MPU-6000-MPU-6050-register-map.pdf` - "MPU-6000 and MPU-6050 Register Map and Descriptions"
  (47 pages, downloaded from `https://cdn.sparkfun.com/datasheets/Sensors/Accelerometers/RM-MPU-6000A.pdf`)

Registers and values used by the driver:

| Address | Register | Usage |
| ------- | -------- | ----- |
| `0x19` | `SMPLRT_DIV` | sample rate divider, `0x04` = 200 Hz |
| `0x1a` | `CONFIG` | digital low pass filter, `0x03` = ~44 Hz bandwidth |
| `0x1b` | `GYRO_CONFIG` | full scale in bits 4:3 - `0` = ±250, `1` = ±500, `2` = ±1000, `3` = ±2000 °/s |
| `0x1c` | `ACCEL_CONFIG` | full scale in bits 4:3 - `0` = ±2, `1` = ±4, `2` = ±8, `3` = ±16 g |
| `0x3b` .. `0x48` | `ACCEL_XOUT_H` .. `GYRO_ZOUT_L` | 14 byte burst read: accel x/y/z, temperature, gyro x/y/z, each a big endian `int16` |
| `0x6b` | `PWR_MGMT_1` | `0x80` device reset, `0x01` wake up with the PLL/X gyroscope clock source |
| `0x75` | `WHO_AM_I` | must read `0x68` (used to detect the chip) |

Scaling factors:

- accelerometer: `16384` / `8192` / `4096` / `2048` LSB per g for ±2 / ±4 / ±8 / ±16 g
- gyroscope: `131` / `65.5` / `32.8` / `16.4` LSB per °/s for ±250 / ±500 / ±1000 / ±2000 °/s
- temperature: `raw / 340 + 36.53` °C

I2C address: `0x68` with AD0 low, `0x69` with AD0 high.
