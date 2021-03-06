/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cutils/log.h>
#include <errno.h>
#include <fcntl.h>
#include <hardware/hardware.h>
#include <hardware/consumerir.h>
#include <malloc.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG "ConsumerIrHal"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define UNUSED __attribute__((unused))

static pthread_mutex_t g_mtx;

static const consumerir_freq_range_t consumerir_freqs[] = {
#ifdef USE_ONE_FREQ_RANGE
    {.min = 16000, .max = 60000},
#else
    {.min = 30000, .max = 30000},
    {.min = 33000, .max = 33000},
    {.min = 36000, .max = 36000},
    {.min = 38000, .max = 38000},
    {.min = 40000, .max = 40000},
    {.min = 56000, .max = 56000},
#endif
};

static int consumerir_transmit_impl(int carrier_freq, const int pattern[],
    int pattern_len)
{
    int buffer_len = 0;
    int buffer_size = 128;
    int fd = open("/sys/class/sec/sec_ir/ir_send", O_RDWR);
    int i;
    char *buffer;

    buffer = malloc(buffer_size);
    if (buffer == NULL)
        return -ENOMEM;

    // Write the header
    if (!append_number(&buffer, &buffer_len, &buffer_size, carrier_freq))
        goto error;

    // Write out the timing pattern
    for (i = 0; i < pattern_len; i++) {
        if (!append_number(&buffer, &buffer_len, &buffer_size, pattern[i]))
            goto error;
    }

    buffer[buffer_len - 1] = 0;
    write(fd, buffer, buffer_len - 1);

    free(buffer);
    close(fd);

    return 0;

error:
    free(buffer);
    return -ENOMEM;
}

static int consumerir_transmit(UNUSED struct consumerir_device *dev,
   int carrier_freq, const int pattern[], int pattern_len)
{
    pthread_mutex_lock(&g_mtx);

    int ret = consumerir_transmit_impl(carrier_freq, pattern, pattern_len);
    if (ret < 0)
        ALOGE("Consumer IR transmit failed. Error: %d", ret);

    pthread_mutex_unlock(&g_mtx);

    return ret;
}

static int consumerir_get_carrier_freqs(UNUSED struct consumerir_device *dev,
    size_t len, consumerir_freq_range_t *ranges)
{
    size_t to_copy = ARRAY_SIZE(consumerir_freqs);

    to_copy = len < to_copy ? len : to_copy;
    memcpy(ranges, consumerir_freqs, to_copy * sizeof(consumerir_freq_range_t));
    return to_copy;
}

static int consumerir_close(hw_device_t *dev)
{
    free(dev);
    pthread_mutex_destroy(&g_mtx);
    return 0;
}

static int consumerir_get_num_carrier_freqs(UNUSED struct consumerir_device *dev)
{
    return ARRAY_SIZE(consumerir_freqs);
}

/*
 * Generic device handling
 */
static int consumerir_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    if (strcmp(name, CONSUMERIR_TRANSMITTER) != 0)
        return -EINVAL;

    if (device == NULL) {
        ALOGE("NULL device on open");
        return -EINVAL;
    }

    consumerir_device_t *dev = malloc(sizeof(consumerir_device_t));
    memset(dev, 0, sizeof(consumerir_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close = consumerir_close;

    dev->transmit = consumerir_transmit;
    dev->get_carrier_freqs = consumerir_get_carrier_freqs;
    dev->get_num_carrier_freqs = consumerir_get_num_carrier_freqs;

    *device = (hw_device_t*) dev;

    pthread_mutex_init(&g_mtx, NULL);

    return 0;
}

static struct hw_module_methods_t consumerir_module_methods = {
    .open = consumerir_open,
};

consumerir_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = CONSUMERIR_MODULE_API_VERSION_1_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = CONSUMERIR_HARDWARE_MODULE_ID,
        .name               = "Consumer IR Module",
        .author             = "The CyanogenMod Project",
        .methods            = &consumerir_module_methods,
    },
};
