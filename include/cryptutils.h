#ifndef __CRYPTUTILS_H
#define __CRYPTUTILS_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifndef MINIMUM_MD5_BUF_SIZE
    #define MINIMUM_MD5_BUF_SIZE ( 32 + 1 )
#endif
#define JMD5_DIGEST_SIZE 16

struct JMD5
{
    uint32_t        m_aState[4];
    uint64_t        m_nCount;
    uint8_t         m_aBuffer[64];
    uint8_t         m_aDigest[JMD5_DIGEST_SIZE];
    bool            m_bFinalized;

    JMD5()
    {
        Reset();
    }

    inline void Reset()
    {
        m_aState[0] = 0x67452301;
        m_aState[1] = 0xEFCDAB89;
        m_aState[2] = 0x98BADCFE;
        m_aState[3] = 0x10325476;
        m_nCount = 0;
        memset(m_aBuffer, 0, sizeof(m_aBuffer));
        memset(m_aDigest, 0, sizeof(m_aDigest));
        m_bFinalized = false;
    }

    inline void Update(const void* data, size_t len)
    {
        if(!data || !len || m_bFinalized) return;

        const uint8_t* input = (const uint8_t*)data;
        uint32_t index = (uint32_t)((m_nCount >> 3) & 0x3F);
        m_nCount += ((uint64_t)len << 3);

        uint32_t partLen = 64 - index;
        size_t i = 0;

        if(len >= partLen)
        {
            memcpy(&m_aBuffer[index], input, partLen);
            Transform(m_aBuffer);

            for(i = partLen; i + 63 < len; i += 64)
            {
                Transform(&input[i]);
            }

            index = 0;
        }

        memcpy(&m_aBuffer[index], &input[i], len - i);
    }

    inline bool UpdateFile(const char* path)
    {
        if(!path || !path[0] || m_bFinalized) return false;

        FILE* file = fopen(path, "rb");
        if(!file) return false;

        uint8_t buffer[1024];
        size_t bytesRead;
        while((bytesRead = fread(buffer, 1, sizeof(buffer), file)))
        {
            Update(buffer, bytesRead);
        }

        fclose(file);
        return true;
    }

    inline void Finalize()
    {
        if(m_bFinalized) return;

        static const uint8_t PADDING[64] = { 0x80 };
        uint8_t bits[8];
        Encode64(bits, m_nCount);

        uint32_t index = (uint32_t)((m_nCount >> 3) & 0x3F);
        uint32_t padLen = (index < 56) ? (56 - index) : (120 - index);

        Update(PADDING, padLen);
        Update(bits, 8);

        Encode32(&m_aDigest[0], m_aState[0]);
        Encode32(&m_aDigest[4], m_aState[1]);
        Encode32(&m_aDigest[8], m_aState[2]);
        Encode32(&m_aDigest[12], m_aState[3]);
        m_bFinalized = true;
    }

    inline const uint8_t* Raw()
    {
        Finalize();
        return m_aDigest;
    }

    inline void Get(char* out, size_t out_len)
    {
        if(!out || out_len < MINIMUM_MD5_BUF_SIZE) return;

        Finalize();

        for(uint8_t i = 0; i < JMD5_DIGEST_SIZE; ++i)
        {
            sprintf(&out[i * 2], "%02x", m_aDigest[i]);
        }
        out[2 * JMD5_DIGEST_SIZE] = 0;
    }

private:
    inline static uint32_t F(uint32_t x, uint32_t y, uint32_t z)
    {
        return (x & y) | (~x & z);
    }

    inline static uint32_t G(uint32_t x, uint32_t y, uint32_t z)
    {
        return (x & z) | (y & ~z);
    }

    inline static uint32_t H(uint32_t x, uint32_t y, uint32_t z)
    {
        return x ^ y ^ z;
    }

    inline static uint32_t I(uint32_t x, uint32_t y, uint32_t z)
    {
        return y ^ (x | ~z);
    }

    inline static uint32_t RotateLeft(uint32_t x, uint32_t n)
    {
        return (x << n) | (x >> (32 - n));
    }

    inline static void FF(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
    {
        a += F(b, c, d) + x + ac;
        a = RotateLeft(a, s);
        a += b;
    }

    inline static void GG(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
    {
        a += G(b, c, d) + x + ac;
        a = RotateLeft(a, s);
        a += b;
    }

    inline static void HH(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
    {
        a += H(b, c, d) + x + ac;
        a = RotateLeft(a, s);
        a += b;
    }

    inline static void II(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
    {
        a += I(b, c, d) + x + ac;
        a = RotateLeft(a, s);
        a += b;
    }

    inline static uint32_t Decode32(const uint8_t* input)
    {
        return ((uint32_t)input[0]) | (((uint32_t)input[1]) << 8) | (((uint32_t)input[2]) << 16) | (((uint32_t)input[3]) << 24);
    }

    inline static void Encode32(uint8_t* output, uint32_t value)
    {
        output[0] = (uint8_t)(value & 0xFF);
        output[1] = (uint8_t)((value >> 8) & 0xFF);
        output[2] = (uint8_t)((value >> 16) & 0xFF);
        output[3] = (uint8_t)((value >> 24) & 0xFF);
    }

    inline static void Encode64(uint8_t* output, uint64_t value)
    {
        output[0] = (uint8_t)(value & 0xFF);
        output[1] = (uint8_t)((value >> 8) & 0xFF);
        output[2] = (uint8_t)((value >> 16) & 0xFF);
        output[3] = (uint8_t)((value >> 24) & 0xFF);
        output[4] = (uint8_t)((value >> 32) & 0xFF);
        output[5] = (uint8_t)((value >> 40) & 0xFF);
        output[6] = (uint8_t)((value >> 48) & 0xFF);
        output[7] = (uint8_t)((value >> 56) & 0xFF);
    }

    inline void Transform(const uint8_t block[64])
    {
        uint32_t a = m_aState[0];
        uint32_t b = m_aState[1];
        uint32_t c = m_aState[2];
        uint32_t d = m_aState[3];
        uint32_t x[16];

        for(uint8_t i = 0; i < 16; ++i)
        {
            x[i] = Decode32(&block[i * 4]);
        }

        FF(a, b, c, d, x[0], 7, 0xd76aa478); FF(d, a, b, c, x[1], 12, 0xe8c7b756); FF(c, d, a, b, x[2], 17, 0x242070db); FF(b, c, d, a, x[3], 22, 0xc1bdceee);
        FF(a, b, c, d, x[4], 7, 0xf57c0faf); FF(d, a, b, c, x[5], 12, 0x4787c62a); FF(c, d, a, b, x[6], 17, 0xa8304613); FF(b, c, d, a, x[7], 22, 0xfd469501);
        FF(a, b, c, d, x[8], 7, 0x698098d8); FF(d, a, b, c, x[9], 12, 0x8b44f7af); FF(c, d, a, b, x[10], 17, 0xffff5bb1); FF(b, c, d, a, x[11], 22, 0x895cd7be);
        FF(a, b, c, d, x[12], 7, 0x6b901122); FF(d, a, b, c, x[13], 12, 0xfd987193); FF(c, d, a, b, x[14], 17, 0xa679438e); FF(b, c, d, a, x[15], 22, 0x49b40821);

        GG(a, b, c, d, x[1], 5, 0xf61e2562); GG(d, a, b, c, x[6], 9, 0xc040b340); GG(c, d, a, b, x[11], 14, 0x265e5a51); GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
        GG(a, b, c, d, x[5], 5, 0xd62f105d); GG(d, a, b, c, x[10], 9, 0x02441453); GG(c, d, a, b, x[15], 14, 0xd8a1e681); GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
        GG(a, b, c, d, x[9], 5, 0x21e1cde6); GG(d, a, b, c, x[14], 9, 0xc33707d6); GG(c, d, a, b, x[3], 14, 0xf4d50d87); GG(b, c, d, a, x[8], 20, 0x455a14ed);
        GG(a, b, c, d, x[13], 5, 0xa9e3e905); GG(d, a, b, c, x[2], 9, 0xfcefa3f8); GG(c, d, a, b, x[7], 14, 0x676f02d9); GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

        HH(a, b, c, d, x[5], 4, 0xfffa3942); HH(d, a, b, c, x[8], 11, 0x8771f681); HH(c, d, a, b, x[11], 16, 0x6d9d6122); HH(b, c, d, a, x[14], 23, 0xfde5380c);
        HH(a, b, c, d, x[1], 4, 0xa4beea44); HH(d, a, b, c, x[4], 11, 0x4bdecfa9); HH(c, d, a, b, x[7], 16, 0xf6bb4b60); HH(b, c, d, a, x[10], 23, 0xbebfbc70);
        HH(a, b, c, d, x[13], 4, 0x289b7ec6); HH(d, a, b, c, x[0], 11, 0xeaa127fa); HH(c, d, a, b, x[3], 16, 0xd4ef3085); HH(b, c, d, a, x[6], 23, 0x04881d05);
        HH(a, b, c, d, x[9], 4, 0xd9d4d039); HH(d, a, b, c, x[12], 11, 0xe6db99e5); HH(c, d, a, b, x[15], 16, 0x1fa27cf8); HH(b, c, d, a, x[2], 23, 0xc4ac5665);

        II(a, b, c, d, x[0], 6, 0xf4292244); II(d, a, b, c, x[7], 10, 0x432aff97); II(c, d, a, b, x[14], 15, 0xab9423a7); II(b, c, d, a, x[5], 21, 0xfc93a039);
        II(a, b, c, d, x[12], 6, 0x655b59c3); II(d, a, b, c, x[3], 10, 0x8f0ccc92); II(c, d, a, b, x[10], 15, 0xffeff47d); II(b, c, d, a, x[1], 21, 0x85845dd1);
        II(a, b, c, d, x[8], 6, 0x6fa87e4f); II(d, a, b, c, x[15], 10, 0xfe2ce6e0); II(c, d, a, b, x[6], 15, 0xa3014314); II(b, c, d, a, x[13], 21, 0x4e0811a1);
        II(a, b, c, d, x[4], 6, 0xf7537e82); II(d, a, b, c, x[11], 10, 0xbd3af235); II(c, d, a, b, x[2], 15, 0x2ad7d2bb); II(b, c, d, a, x[9], 21, 0xeb86d391);

        m_aState[0] += a;
        m_aState[1] += b;
        m_aState[2] += c;
        m_aState[3] += d;

        memset(x, 0, sizeof(x));
    }
};

#endif // __CRYPTUTILS_H
