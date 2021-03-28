//
// Created by dbrent on 3/1/21.
//

#ifndef DBSDRT_CONVENIENCE_H
#define DBSDRT_CONVENIENCE_H

#include <SoapySDR/Device.h>
#include <stdint.h>

/* a collection of user friendly tools */

/*!
 * Convert standard suffixes (k, M, G) to double
 *
 * \param s a string to be parsed
 * \return double
 */

double atofs(char *s);

/*!
 * Convert time suffixes (s, m, h) to double
 *
 * \param s a string to be parsed
 * \return seconds as double
 */

double atoft(char *s);

/*!
 * Convert percent suffixe (%) to double
 *
 * \param s a string to be parsed
 * \return double
 */

double atofp(char *s);

/*!
 * Set device frequency and report status on stderr
 *
 * \param dev the device handle
 * \param frequency in Hz
 * \return 0 on success
 */

int verbose_set_frequency(SoapySDRDevice *dev, uint32_t frequency,
                          size_t channel);

/*!
 * Set device sample rate and report status on stderr
 *
 * \param dev the device handle
 * \param samp_rate in samples/second
 * \return 0 on success
 */

int verbose_set_sample_rate(SoapySDRDevice *dev, uint32_t samp_rate,
                            size_t channel);

/*!
 * Set device bandwidth and report status on stderr
 *
 * \param dev the device handle
 * \param frequency in Hz
 * \return 0 on success
 */

int verbose_set_bandwidth(SoapySDRDevice *dev, uint32_t bandwidth,
                          size_t channel);

/*!
 * Enable or disable the direct sampling mode and report status on stderr
 *
 * \param dev the device handle
 * \param on 0 means disabled, 1 I-ADC input enabled, 2 Q-ADC input enabled
 * \return 0 on success
 */

int verbose_direct_sampling(SoapySDRDevice *dev, int on);

/*!
 * Enable offset tuning and report status on stderr
 *
 * \param dev the device handle
 * \return 0 on success
 */

int verbose_offset_tuning(SoapySDRDevice *dev);

/*!
 * Enable auto gain and report status on stderr
 *
 * \param dev the device handle
 * \return 0 on success
 */

int verbose_auto_gain(SoapySDRDevice *dev, size_t channel);

/*!
 * Set tuner gain and report status on stderr
 *
 * \param dev the device handle
 * \param gain in tenths of a dB
 * \return 0 on success
 */

int verbose_gain_set(SoapySDRDevice *dev, int gain);

/*!
 * Set tuner gain elements by a key/value string
 *
 * \param dev the device handle
 * \param gain_str string of gain element pairs (example LNA=40,VGA=20,AMP=0),
 * or string of overall gain, in dB \return 0 on success
 */
int verbose_gain_str_set(SoapySDRDevice *dev, char *gain_str, size_t channel);

/*!
 * Set antenna and channel
 * \param dev the device handle
 * \param channel number of channel (0 is default)
 * \param antenna_str name of the antenna as reported by SoapySDR
 */
int verbose_antenna_str_set(SoapySDRDevice *dev, int channel,
                            char *antenna_str);

/*!
 * Set the frequency correction value for the device and report status on
 * stderr.
 *
 * \param dev the device handle
 * \param ppm_error correction value in parts per million (ppm)
 * \return 0 on success
 */
int verbose_ppm_set(SoapySDRDevice *dev, int ppm_error, size_t channel);

/*!
 * Reset buffer
 *
 * \param dev the device handle
 * \return 0 on success
 */

int verbose_reset_buffer(SoapySDRDevice *dev);

/*!
 * Find the closest matching device.
 *
 * \param s a string to be parsed
 * \param devOut device output returned
 * \return dev 0 if successful
 */
int verbose_device_search(char *s, SoapySDRDevice **devOut);

/*!
 * Open a stream on the pre-configured device
 *
 * \param streamOut stream output returned
 * \param channel channel to listen
 * \param format stream format (such as SOAPY_SDR_CS16)
 * \return dev 0 if successful
 */
int verbose_setup_stream(SoapySDRDevice *dev, SoapySDRStream **streamOut,
                         size_t channel, const char *format);

/*!
 * Apply settings to device
 *
 * \param dev the device handle
 * \param sdr_settings settings to apply
 * \return 0 on success
 */

int verbose_settings(SoapySDRDevice *dev, const char *sdr_settings);

/*!
 * Start redirecting stdout to stderr to avoid unwanted stdout emissions.
 * Applications should call this if they want to use stdout for their own
 * output, before verbose_device_start(), and optionally stop after configuring
 * all settings.
 *
 * \return Saved file descriptor to pass to suppress_stdout_stop()
 */
int suppress_stdout_start(void);

/*!
 * Stop redirecting stdout to stderr.
 *
 * \param tmp_stdout File descriptor from suppress_stdout_start()
 */
void suppress_stdout_stop(int tmp_stdout);

#endif // DBSDRT_CONVENIENCE_H
