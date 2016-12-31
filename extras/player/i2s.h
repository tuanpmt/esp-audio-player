#ifndef _I2S_H_
#define _I2S_H_
#ifdef __cplusplus
extern "C" {
#endif
void i2s_init(int sample_rate);
void i2s_push_sample(int sample);
void i2s_slient();
void i2s_stop();

#ifdef __cplusplus
}
#endif
#endif
