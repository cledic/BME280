/*
*/
#include "atmel_start.h"
#include "atmel_start_pins.h"

#include <hal_i2c_m_sync.h>
#include <utils.h>
#include <utils_assert.h>

#include "BME280_lib.h"

/**
 * \brief Driver version
 */
#define BME280_DRIVER_VERSION 0x00000001u

/**
 * \brief Private functions
 */
int32_t read_RegisterMultiValue(struct i2c_m_sync_desc *i2c, uint8_t reg, uint8_t *value, const uint16_t n);
int32_t BME280_ReadCalibrationData( void);

struct io_descriptor *I2C_0_io;
CALIBRATION_VALUES	clbr;
uint32_t			BME280_Init_Done=0;
int32_t				ret=0;
int32_t				t_fine;
uint8_t				buffer[BME280_CALIB1_SZ+BME280_CALIB2_SZ];

/**
 * \brief Initialize low lever I2C interface and read the BME280 chip ID
 *
 * \param[in] none
 * \return Correct identification.
 * \retval 0 Identification OK
 * \retval return the error code 
 */
int32_t BME280_Init(void)
{
	
	i2c_m_sync_get_io_descriptor( &I2C_0, &I2C_0_io);
	i2c_m_sync_enable( &I2C_0);
	i2c_m_sync_set_slaveaddr( &I2C_0, BME280_ADDR, I2C_M_SEVEN);
	
	ret=i2c_m_sync_cmd_read( &I2C_0, BME280_REG_ID, &buffer[0]);
	if ( buffer[0] == BME280_CHIPID) {
		/* */
		BME280_Init_Done=1;
		BME280_ReadCalibrationData();
		/* set default profile */
		BME280_SetProfile( BME280_PROFILE_WEATHER);
		return 0;
	} else {
		return ret;
	}
}

int32_t BME280_Get_Temperature( float*t)
{
	uint32_t v;
	int32_t var1, var2;
	float T;
	int32_t mode, status;
	
	if ( BME280_Init_Done==0)
		return 1;
	
	ret = BME280_GetSensorMode( &mode);
	if ( ret != 0)
		return ret;
	
	if ( mode==BME280_MODE_FORCED || mode==BME280_MODE_SLEEP) {
		ret = BME280_SetSensorMode( BME280_MODE_FORCED);
		if ( ret != 0)
			return ret;
	}
	
	do {
		ret = BME280_GetStatus( &status);
		if ( ret != 0)
			return ret;
	} while( status);
	
	ret = read_RegisterMultiValue( &I2C_0, BME280_REG_TEMP_MSB, &buffer[0], BME280_TEMP_SZ);
	
	v = (buffer[0]<<16) | (buffer[1]<<8) | buffer[2];
	v = (v>>4);

	var1 = ((((v>>3) - ((int32_t)clbr.dig_T1<<1))) * ((int32_t)clbr.dig_T2)) >> 11;
	var2 = (((((v>>4) - ((int32_t)clbr.dig_T1)) * ((v>>4) - ((int32_t)clbr.dig_T1))) >> 12) * ((int32_t)clbr.dig_T3)) >> 14;
	t_fine = var1 + var2;
	T = (t_fine * 5 + 128) >> 8;
	*t=(float)T/100;
	
	return ret;
}

int32_t BME280_Get_Pressure( float*p)
{
	uint32_t v;
	int64_t var1, var2, P;
	int32_t mode, status;
	
	if ( BME280_Init_Done==0)
		return 1;

	ret = BME280_GetSensorMode( &mode);
	if ( ret != 0)
	return ret;
	
	if ( mode==BME280_MODE_FORCED || mode==BME280_MODE_SLEEP) {
		ret = BME280_SetSensorMode( BME280_MODE_FORCED);
		if ( ret != 0)
		return ret;
	}
	
	do {
		ret = BME280_GetStatus( &status);
		if ( ret != 0)
		return ret;
	} while( status);
	
	ret = read_RegisterMultiValue( &I2C_0, BME280_REG_PRESS_MSB, &buffer[0], BME280_PRESS_SZ);
	
	v = (buffer[0]<<16) | (buffer[1]<<8) | buffer[2];
	v = (v>>4);

	var1 = ((int64_t)t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)clbr.dig_P6;
	var2 = var2 + ((var1*(int64_t)clbr.dig_P5)<<17);
	var2 = var2 + (((int64_t)clbr.dig_P4)<<35);
	var1 = ((var1 * var1 * (int64_t)clbr.dig_P3)>>8) + ((var1 * (int64_t)clbr.dig_P2)<<12);
	var1 = (((((int64_t)1)<<47)+var1))*((int64_t)clbr.dig_P1)>>33;
	if (var1 == 0)
	{
		return 0; // avoid exception caused by division by zero
	}
	P = 1048576-v;
	P = (((P<<31)-var2)*3125)/var1;
	var1 = (((int64_t)clbr.dig_P9) * (P>>13) * (P>>13)) >> 25;
	var2 = (((int64_t)clbr.dig_P8) * P) >> 19;
	P = ((P + var1 + var2) >> 8) + (((int64_t)clbr.dig_P7)<<4);
	
	*p = (float)P/256;
	
	return ret;
}

int32_t BME280_Get_Humidity( float*h)
{
	uint32_t v;
	int32_t v_x1_u32r;
	float H;
	int32_t mode, status;
	
	if ( BME280_Init_Done==0)
		return 1;

	ret = BME280_GetSensorMode( &mode);
	if ( ret != 0)
	return ret;
	
	if ( mode==BME280_MODE_FORCED || mode==BME280_MODE_SLEEP) {
		ret = BME280_SetSensorMode( BME280_MODE_FORCED);
		if ( ret != 0)
		return ret;
	}
	
	do {
		ret = BME280_GetStatus( &status);
		if ( ret != 0)
		return ret;
	} while( status);
	
	ret = read_RegisterMultiValue( &I2C_0, BME280_REG_HUM_MSB, &buffer[0], BME280_HUM_SZ);
	
	v = (buffer[0]<<8) | buffer[1];
	
	v_x1_u32r = (t_fine - ((int32_t)76800));
	v_x1_u32r = (((((v << 14) - (((int32_t)clbr.dig_H4) << 20) - (((int32_t)clbr.dig_H5) * v_x1_u32r)) + \
	((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)clbr.dig_H6)) >> 10) * (((v_x1_u32r * \
	((int32_t)clbr.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * \
	((int32_t)clbr.dig_H2) + 8192) >> 14));
	v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)clbr.dig_H1)) >> 4));
	v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
	v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
	
	H = (v_x1_u32r>>12);
	*h = H/1024;
	
	return ret;
}

 
int32_t BME280_GetStatus( int32_t*s)
{
	if ( BME280_Init_Done==0)
	return 1;
	
	ret = i2c_m_sync_cmd_read( &I2C_0, BME280_REG_STATUS, &buffer[0]);
	
	if ( ret==0)
		*s=(int32_t)buffer[0];
	
	return ret;
}
 
int32_t BME280_ReadCalibrationData( void)
{
	int i;
	
	if ( BME280_Init_Done==0)
		return 1;
	
	ret = read_RegisterMultiValue( &I2C_0, BME280_REG_CALIB1, &buffer[0], BME280_CALIB1_SZ);
	ret = read_RegisterMultiValue( &I2C_0, BME280_REG_CALIB2, &buffer[BME280_CALIB1_SZ], BME280_CALIB2_SZ);

	clbr.dig_T1 = (buffer[1]<<8) | buffer[0];
	clbr.dig_T2 = (buffer[3]<<8) | buffer[2];
	clbr.dig_T3 = (buffer[5]<<8) | buffer[4];
	clbr.dig_P1 = (buffer[7]<<8) | buffer[6];
	clbr.dig_P2 = (buffer[9]<<8) | buffer[8];
	clbr.dig_P3 = (buffer[11]<<8) | buffer[10];
	clbr.dig_P4 = (buffer[13]<<8) | buffer[12];
	clbr.dig_P5 = (buffer[15]<<8) | buffer[14];
	clbr.dig_P6 = (buffer[17]<<8) | buffer[16];
	clbr.dig_P7 = (buffer[19]<<8) | buffer[18];
	clbr.dig_P8 = (buffer[21]<<8) | buffer[20];
	clbr.dig_P9 = (buffer[23]<<8) | buffer[22];
	clbr.dig_H1 = buffer[24];
	clbr.dig_H2 = (buffer[26]<<8) | buffer[25];
	clbr.dig_H3 = buffer[27];
	clbr.dig_H4 = (buffer[28]<<4) | (buffer[29] & 0xF);
	clbr.dig_H5 = (buffer[30]<<4) | (buffer[29]>>4);
	clbr.dig_H6 = buffer[31];

	for ( i=0; i<(BME280_CALIB1_SZ+BME280_CALIB2_SZ);i++) {
		buffer[i]=0;
	}
	
	return 0;
}

int32_t BME280_SetTempOversampling( int32_t to)
{
	uint8_t value;
	
	ret = i2c_m_sync_cmd_read( &I2C_0, BME280_REG_CTRL_MEAS, &value);
	
	if ( ret != 0)
		return ret;
		
	value = value & (~BME280_TEMP_OVRSMPL_MASK);
	value = value | to;
	
	ret = i2c_m_sync_cmd_write( &I2C_0, BME280_REG_CTRL_MEAS, value);
	
	return ret;
}

int32_t BME280_SetPressOversampling( int32_t po)
{
	uint8_t value;
	
	ret = i2c_m_sync_cmd_read( &I2C_0, BME280_REG_CTRL_MEAS, &value);
	
	if ( ret != 0)
		return ret;
		
	value = value & (~BME280_PRESS_OVRSMPL_MASK);
	value = value | po;
	
	ret = i2c_m_sync_cmd_write( &I2C_0, BME280_REG_CTRL_MEAS, value);
	
	return ret;
}

int32_t BME280_SetHumOversampling( int32_t ho)
{
	uint8_t value;
	
	ret = i2c_m_sync_cmd_read( &I2C_0, BME280_REG_CTRL_HUM, &value);
	
	if ( ret != 0)
		return ret;
		
	value = value & (~BME280_HUM_OVRSMPL_MASK);
	value = value | ho;
	
	ret = i2c_m_sync_cmd_write( &I2C_0, BME280_REG_CTRL_HUM, value);
	
	return ret;
}

int32_t BME280_SetInactiveDuration( int32_t id)
{
	uint8_t value;
	
	ret = i2c_m_sync_cmd_read( &I2C_0, BME280_REG_CONFIG, &value);
	
	if ( ret != 0)
		return ret;
		
	value = value & (~BME280_TIME_MASK);
	value = value | id;
	
	ret = i2c_m_sync_cmd_write( &I2C_0, BME280_REG_CONFIG, value);
	
	return ret;
}

int32_t BME280_SetFilterCoeff( int32_t fc)
{
	uint8_t value;
	
	ret = i2c_m_sync_cmd_read( &I2C_0, BME280_REG_CONFIG, &value);
	
	if ( ret != 0)
		return ret;
		
	value = value & (~BME280_FLTRCOEFF_MASK);
	value = value | fc;
	
	ret = i2c_m_sync_cmd_write( &I2C_0, BME280_REG_CONFIG, value);
	
	return ret;
}

/**
  * \brief	Set a Profile for the device.
 *			Possible values are: 
 *			BME280_PROFILE_WEATHER		(0.16 uA)
 *			BME280_PROFILE_HUMIDITY		(2.9  uA)
 *			BME280_PROFILE_INDOOR		(633  uA)
 *			BME280_PROFILE_GAMING		(581  uA)
 *
 * \param[in]	int32_t p	profile 
 * \return 		return 0 for success, or other values for error
 */
int32_t BME280_SetProfile( int32_t p)
{
	switch (p) {
		case BME280_PROFILE_WEATHER:
			/*
			 * Performance for suggested settings
			 * Current consumption 0.16 uA
			 * RMS Noise 3.3 Pa / 30 cm, 0.07 %RH
			 * Data output rate 1/60 Hz
			 */
			BME280_SetTempOversampling( BME280_TEMP_OVRSMPL_x1);
			BME280_SetPressOversampling( BME280_PRESS_OVRSMPL_x1);
			BME280_SetHumOversampling( BME280_HUM_OVRSMPL_x1);
			BME280_SetFilterCoeff( BME280_FLTRCOEFF_OFF);
			BME280_SetInactiveDuration( BME280_TIME_SNDBY_1000);
			BME280_SetSensorMode( BME280_MODE_FORCED);
		break;
		case BME280_PROFILE_HUMIDITY:
			/*
			 * Performance for suggested settings
			 * Current consumption 2.9 uA
			 * RMS Noise 0.07 %RH
			 * Data output rate 1 Hz
			 */
			BME280_SetTempOversampling( BME280_TEMP_OVRSMPL_x1);
			BME280_SetPressOversampling( BME280_PRESS_OVRSMPL_SKIPPED);
			BME280_SetHumOversampling( BME280_HUM_OVRSMPL_x1);
			BME280_SetFilterCoeff( BME280_FLTRCOEFF_OFF);
			BME280_SetInactiveDuration( BME280_TIME_SNDBY_1000);		
			BME280_SetSensorMode( BME280_MODE_FORCED);
		break;
		case BME280_PROFILE_INDOOR:
			/*
			 * Current consumption 633 uA
			 * RMS Noise 0.2 Pa / 1.7 cm
			 * Data output rate 25Hz
			 * Filter bandwidth 0.53 Hz
			 * Response time (75%) 0.9 s
			 */
			BME280_SetTempOversampling( BME280_TEMP_OVRSMPL_x2);
			BME280_SetPressOversampling(BME280_PRESS_OVRSMPL_x16);
			BME280_SetHumOversampling( BME280_HUM_OVRSMPL_x1);
			BME280_SetFilterCoeff( BME280_FLTRCOEFF_16);
			BME280_SetInactiveDuration( BME280_TIME_SNDBY_0_5);				
			BME280_SetSensorMode( BME280_MODE_NORMAL);
		break;
		case BME280_PROFILE_GAMING:
			/*
			 * Current consumption 581 uA
			 * RMS Noise 0.3 Pa / 2.5 cm
			 * Data output rate 83 Hz
			 * Filter bandwidth 1.75 Hz
			 * Response time (75%) 0.3 s
			 */
			BME280_SetTempOversampling( BME280_TEMP_OVRSMPL_x1);
			BME280_SetPressOversampling(BME280_PRESS_OVRSMPL_x4);
			BME280_SetHumOversampling( BME280_HUM_OVRSMPL_SKIPPED);
			BME280_SetFilterCoeff( BME280_FLTRCOEFF_16);
			BME280_SetInactiveDuration( BME280_TIME_SNDBY_0_5);						
			BME280_SetSensorMode( BME280_MODE_NORMAL);
		break;
		default:
			/* default to weater profile... */
			BME280_SetTempOversampling( BME280_TEMP_OVRSMPL_x1);
			BME280_SetPressOversampling( BME280_PRESS_OVRSMPL_x1);
			BME280_SetHumOversampling( BME280_HUM_OVRSMPL_x1);
			BME280_SetFilterCoeff( BME280_FLTRCOEFF_OFF);
			BME280_SetInactiveDuration( BME280_TIME_SNDBY_1000);		
			BME280_SetSensorMode( BME280_MODE_FORCED);
		break;
	}
	
	return 0;
}

int32_t BME280_SetSensorMode( int32_t sm)
{
	uint8_t value;
	
	ret = i2c_m_sync_cmd_read( &I2C_0, BME280_REG_CTRL_MEAS, &value);
	
	if ( ret != 0)
		return ret;
		
	value = value & (~BME280_MODE_MASK);
	value = value | sm;
	
	ret = i2c_m_sync_cmd_write( &I2C_0, BME280_REG_CTRL_MEAS, value);
	
	return ret;
}

int32_t BME280_GetSensorMode( int32_t*sm)
{
	uint8_t value;
	
	ret = i2c_m_sync_cmd_read( &I2C_0, BME280_REG_CTRL_MEAS, &value);
	
	if ( ret != 0)
	return ret;

	*sm=(int32_t)value;
	
	return ret;
}

int32_t read_RegisterMultiValue(struct i2c_m_sync_desc *i2c, uint8_t reg, uint8_t *value, const uint16_t n)
{
	struct _i2c_m_msg msg;

	msg.addr = i2c->slave_addr;
	msg.len = 1;
	msg.flags = 0;
	msg.buffer = &reg;

	ret = _i2c_m_sync_transfer(&i2c->device, &msg);

	if (ret != 0) {
		/* error occurred */
		return ret;
	}

	msg.flags = I2C_M_STOP | I2C_M_RD;
	msg.len = n;
	msg.buffer = value;

	ret = _i2c_m_sync_transfer(&i2c->device, &msg);

	return ( ((int32_t)n) > i2c->device.service.msg.len ) ?
	(((int32_t)n) - i2c->device.service.msg.len) : ret;
}


