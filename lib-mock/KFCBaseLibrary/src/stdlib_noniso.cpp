
#include "util/stdlib_noniso.h"


// ulltoa fills str backwards and can return a pointer different from str
char *ulltoa(unsigned long long val, char *str, int slen, unsigned int radix)
{
    str += --slen;
    *str = 0;
    do
    {
        auto mod = val % radix;
        val /= radix;
        *--str = mod + ((mod > 9) ? ('a' - 10) : '0');
    } while (--slen && val);
    return val ? nullptr : str;
}

// lltoa fills str backwards and can return a pointer different from str
char *lltoa(long long val, char *str, int slen, unsigned int radix)
{
    bool neg;
    if (val < 0)
    {
        val = -val;
        neg = true;
    }
    else
    {
        neg = false;
    }
    char *ret = ulltoa(val, str, slen, radix);
    if (neg)
    {
        if (ret == str || ret == nullptr)
            return nullptr;
        *--ret = '-';
    }
    return ret;
}
