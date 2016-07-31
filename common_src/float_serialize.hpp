#ifndef FLOAT_SERIALIZE_HPP
#define FLOAT_SERIALIZE_HPP

// TODO

template<typename S>
S to_signed(std::make_unsigned_t<S> u)
{
    static_assert(std::is_signed<S>::value);
    using U = std::make_unsigned_t<S>;
    using L = std::numeric_limits<S>;
    
    if(u <= L::max())
        return static_cast<S>(u);
        
    if(u >= static_cast<U>(L::min()))
        return static_cast<S>(u - L::min()) + L::max();
        
    throw std::runtime_error("bad to_signed cast");
}

struct float_serialized_t
{
    std::uint16_t exponent;
    std::uint32_t mantissa;
};

float_serialized_t write(float number)
{
    int exponent;
    std::uint32_t mantissa = 0;
    if(std::isinf(number))
        exponent = UINT16_MAX >> 1;
    else if(std::isnan(number))
        exponent = (UINT16_MAX >> 1) - 1;
    else
        mantissa = UINT32_MAX * std::abs(frexp(number, &exponent));
    std::uint16_t exponent16 = exponent;
    exponent16 = (exponent16 << 1) | std::signbit(number);
    return { exponent16, mantissa };
}

float read(float_serialized_t fp)
{
    float f;
    if(fp.exponent >> 1 == UINT16_MAX >> 1)
        f = INFINITY;
    else if(fp.exponent >> 1 == (UINT16_MAX >> 1) - 1)
        f = NAN;
    else
        f = std::ldexp((float)fp.mantissa / UINT32_MAX, 
                       to_signed<std::int16_t>(fp.exponent) >> 1);
    return (fp.exponent & 1) ? -f : f;
}

#endif
