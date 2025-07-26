#pragma once
#include <cstdint>

namespace xorshift
{
    class xorshift32
    {
    private:
        uint32_t m_seed;

    public:
        inline xorshift32(uint32_t _Seed = 1) : m_seed(_Seed) {}
        inline void seed(uint32_t _Seed) { m_seed = _Seed; }
        inline void discard(uint64_t z);
        inline uint32_t operator()();
    };

    inline void xorshift32::discard(uint64_t z)
    {
        while (z--)
            this->operator()();
    }

    inline uint32_t xorshift32::operator()()
    {
        m_seed ^= m_seed << 13;
        m_seed ^= m_seed >> 17;
        m_seed ^= m_seed << 15;
        return m_seed;
    }

    class xorshift64
    {
    private:
        uint64_t m_seed;

    public:
        inline xorshift64(uint64_t _Seed = 1) : m_seed(_Seed) {}
        inline void seed(uint64_t seed) { m_seed = seed; }
        inline void discard(uint64_t z);
        inline uint64_t operator()();
    };

    inline void xorshift64::discard(uint64_t z)
    {
        while (z--)
            this->operator()();
    }

    inline uint64_t xorshift64::operator()()
    {
        m_seed ^= m_seed << 13;
        m_seed ^= m_seed >> 7;
        m_seed ^= m_seed << 17;
        return m_seed;
    }

} // namespace xorshift
