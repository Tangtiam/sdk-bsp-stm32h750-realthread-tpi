/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-05-25     chenyong     first version
 */

#include <mcf.h>
#include <mcf_trans.h>

#ifdef MCF_SECURITY_FASTLZ
#include <fastlz.h>
#endif

#ifdef MCF_SECURITY_AES256
#include <tiny_aes.h>
#endif

#define DBG_TAG               "mcf.sec"
#ifdef MCF_USING_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif /* MCF_DEBUG */
#include <rtdbg.h>

#ifdef MCF_SECURITY_FASTLZ
#define MCF_FASTLZ_COMPRESS_LEVEL      1
#define MCF_FASTLZ_BUFFER_PADDING      FASTLZ_BUFFER_PADDING(MCF_PKT_MAX_SIZE)
#define MCF_FASTLZ_MAX_SIZE            (MCF_PKT_MAX_SIZE + MCF_FASTLZ_BUFFER_PADDING)
#endif

#ifdef MCF_SECURITY_AES256
#ifndef MCF_SECURITY_AES_IV
#define MCF_SECURITY_AES_IV  "0123456789ABCDEF"
#endif
#ifndef MCF_SECURITY_AES_KEY
#define MCF_SECURITY_AES_KEY "0123456789ABCDEF0123456789ABCDEF"
#endif
#endif

/* MCF data encrypt feature */
static mcf_err_t mcf_encrypt(unsigned char *buffer, size_t *buf_len)
{
#ifdef MCF_SECURITY_AES256
    tiny_aes_context ctx;
    uint8_t iv[16 + 1];
    uint8_t private_key[32 + 1];
    unsigned char *in_buf = RT_NULL;
    size_t in_buf_len = MCF_AES_ALIGN(*buf_len);

    /* allocate input and output buffer */
    in_buf = rt_calloc(1, in_buf_len );
    if (in_buf == RT_NULL)
    {
        return -MCF_MEM_FULL;
    }
    rt_memcpy(in_buf, buffer, (*buf_len));

    /* set encrypt information */
    rt_memcpy(iv, MCF_SECURITY_AES_IV, rt_strlen(MCF_SECURITY_AES_IV));
    iv[sizeof(iv) - 1] = '\0';
    rt_memcpy(private_key, MCF_SECURITY_AES_KEY, rt_strlen(MCF_SECURITY_AES_KEY));
    private_key[sizeof(private_key) - 1] = '\0';
    tiny_aes_setkey_enc(&ctx, (uint8_t *) private_key, 256);

    /* data encrypt */
    tiny_aes_crypt_cbc(&ctx, AES_ENCRYPT, in_buf_len, iv, in_buf, buffer + MCF_AES_DATA_HEAD);

    /* set out buffer length header information */
    buffer[0] = (*buf_len) << 8;
    buffer[1] = (*buf_len) & 0xFF;
    *buf_len = in_buf_len + MCF_AES_DATA_HEAD;

    rt_free(in_buf);
#endif /* MCF_SECURITY_AES256 */

    return MCF_OK;
}

/* MCF data decrypt feature */
static mcf_err_t mcf_decrypt(unsigned char *buffer, size_t *buf_len)
{
#ifdef MCF_SECURITY_AES256
    tiny_aes_context ctx;
    uint8_t iv[16 + 1];
    uint8_t private_key[32 + 1];
    unsigned char *in_buf = RT_NULL;
    size_t in_buf_len = (*buf_len) - MCF_AES_DATA_HEAD;
    size_t out_buf_len = buffer[0] << 8 | buffer[1];

    /* allocate input and output buffer */
    in_buf = rt_calloc(1, in_buf_len);
    if (in_buf == RT_NULL)
    {
        return -MCF_MEM_FULL;
    }
    rt_memcpy(in_buf, buffer + MCF_AES_DATA_HEAD, in_buf_len);

    /* set encrypt information */
    rt_memcpy(iv, MCF_SECURITY_AES_IV, rt_strlen(MCF_SECURITY_AES_IV));
    iv[sizeof(iv) - 1] = '\0';
    rt_memcpy(private_key, MCF_SECURITY_AES_KEY, rt_strlen(MCF_SECURITY_AES_KEY));
    private_key[sizeof(private_key) - 1] = '\0';
    tiny_aes_setkey_dec(&ctx, (uint8_t *) private_key, 256);

    /* data encrypt */
    rt_memset(buffer, 0x00, (*buf_len));
    tiny_aes_crypt_cbc(&ctx, AES_DECRYPT, in_buf_len, iv, in_buf, buffer);

    /* set out buffer length */
    *buf_len = out_buf_len;

    rt_free(in_buf);
#endif /* MCF_SECURITY_AES256 */

    return MCF_OK ;
}

/* MCF data compress feature */
static mcf_err_t mcf_compress(unsigned char *buffer, size_t *buf_len)
{
#ifdef MCF_SECURITY_FASTLZ
    {
        char *out_buffer = RT_NULL;
        size_t out_buf_len = 0;

        /* allocate out buffer */
        out_buffer = rt_calloc(1, MCF_FASTLZ_MAX_SIZE);
        if (out_buffer == RT_NULL)
        {
            return -MCF_MEM_FULL;
        }

        /* data compress */
        out_buf_len = fastlz_compress_level(MCF_FASTLZ_COMPRESS_LEVEL, buffer, (*buf_len), out_buffer);
        if (out_buf_len < 0)
        {
            rt_free(out_buffer);
            return -MCF_FAILD;
        }

        /* compress data copy to input buffer */
        rt_memset(buffer, 0x00, (*buf_len));
        rt_memcpy(buffer, out_buffer, out_buf_len);
        *buf_len = out_buf_len;
    }
#endif /* MCF_SECURITY_FASTLZ */

    return MCF_OK;
}

/* MCF data decompress feature */
static mcf_err_t mcf_decompress(unsigned char *buffer, size_t *buf_len)
{
#ifdef MCF_SECURITY_FASTLZ
    {
        char *out_buffer = RT_NULL;
        size_t out_buf_len = 0;

        /* allocate out buffer */
        out_buffer = rt_calloc(1, MCF_FASTLZ_MAX_SIZE);
        if (out_buffer == RT_NULL)
        {
            return -MCF_MEM_FULL;
        }

        /* data compress */
        out_buf_len = fastlz_decompress(buffer, (*buf_len), out_buffer, MCF_FASTLZ_MAX_SIZE);
        if (out_buf_len < 0)
        {
            rt_free(out_buffer);
            return -MCF_FAILD;
        }

        /* compress data copy to input buffer */
        rt_memset(buffer, 0x00, (*buf_len));
        rt_memcpy(buffer, out_buffer, out_buf_len);
        *buf_len = out_buf_len;
    }
#endif /* MCF_SECURITY_FASTLZ */

    return MCF_OK;
}

/* initialize MCF security feature */
mcf_err_t mcf_security_init(void)
{
    return MCF_OK;
}

/* MCF data encrypt and compress */
mcf_err_t mcf_security_pack(void *input, size_t in_len, void **output, size_t *out_len)
{
    unsigned char *buffer = RT_NULL;
    size_t buf_len = 0;

#if defined(MCF_SECURITY_FASTLZ) && defined(MCF_SECURITY_AES256)
    buf_len = MCF_AES_ALIGN(in_len) + MCF_AES_DATA_HEAD +
            FASTLZ_BUFFER_PADDING(MCF_AES_ALIGN(in_len) + MCF_AES_DATA_HEAD);
#elif defined(MCF_SECURITY_FASTLZ)
    buf_len = in_len + FASTLZ_BUFFER_PADDING(in_len);
#elif defined(MCF_SECURITY_AES256)
    buf_len = MCF_AES_ALIGN(in_len) + MCF_AES_DATA_HEAD;
#endif

    buffer = rt_calloc(1, buf_len);
    if (buffer == RT_NULL)
    {
        LOG_E("no memory for data security out buffer allocate.");
        return -MCF_MEM_FULL;
    }

    /* copy input buffer to data buffer */
    rt_memcpy(buffer, input, in_len);

    /* data encrypt */
    buf_len = in_len;
    if (mcf_encrypt(buffer, &buf_len) != MCF_OK)
    {
        LOG_E("MCF data encrypt failed.");
        rt_free(buffer);
        return  -MCF_FAILD;
    }

    /* data compress */
    if (mcf_compress(buffer, &buf_len) != MCF_OK)
    {
        LOG_E("MCF data compress failed.");
        rt_free(buffer);
        return -MCF_FAILD;
    }

    /* set out buffer and length */
    *output = buffer;
    *out_len = buf_len;

    return MCF_OK;
}

/* MCF data decompress and decrypt */
mcf_err_t mcf_security_unpack(void *input, size_t in_len, void **output, size_t *out_len)
{
    unsigned char *buffer = RT_NULL;
    size_t buf_len = 0;

#if defined(MCF_SECURITY_FASTLZ) && defined(MCF_SECURITY_AES256)
    buf_len = MCF_AES_ALIGN(in_len) + MCF_AES_DATA_HEAD +
            FASTLZ_BUFFER_PADDING(MCF_AES_ALIGN(in_len) + MCF_AES_DATA_HEAD);
#elif defined(MCF_SECURITY_FASTLZ)
    buf_len = in_len + FASTLZ_BUFFER_PADDING(in_len);
#elif defined(MCF_SECURITY_AES256)
    buf_len = MCF_AES_ALIGN(in_len) + MCF_AES_DATA_HEAD;
#endif

    buffer = rt_calloc(1, buf_len);
    if (buffer == RT_NULL)
    {
        LOG_E("no memory for data security out buffer allocate.");
        return -MCF_MEM_FULL;
    }

    /* copy input buffer to data buffer */
    rt_memcpy(buffer, input, in_len);

    /* data decompress */
    if (mcf_decompress(buffer, &buf_len) != MCF_OK)
    {
        LOG_E("MCF data compress failed.");
        rt_free(buffer);
        return -MCF_FAILD;
    }

    /* data decrypt */
    if (mcf_decrypt(buffer, &buf_len) != MCF_OK)
    {
        LOG_E("MCF data encrypt failed.");
        rt_free(buffer);
        return  -MCF_FAILD;
    }

    /* set out buffer and length */
    *output = buffer;
    *out_len = buf_len;

    return MCF_OK;
}
