#ifndef STORM_UTILITY_MATH_H_
#define STORM_UTILITY_MATH_H_

#include <cmath>

#include "src/utility/OsDetection.h"

namespace storm {
    namespace utility {
        namespace math {
            // We provide this method explicitly, because MSVC does not offer it (non-C99 compliant).
            template<typename ValueType>
            static inline double log2(ValueType number) {
#		ifndef WINDOWS
                return std::log2(number);
#		else
                return std::log(number) / std::log(2);
#		endif
            }
            
            inline uint64_t uint64_log2(uint64_t n)
            {
                assert(n != 0);
                #define S(k) if (n >= (UINT64_C(1) << k)) { i += k; n >>= k; }
                uint64_t i = 0; S(32); S(16); S(8); S(4); S(2); S(1); return i;
                #undef S
            }
        }
    }
}

#endif /* STORM_UTILITY_MATH_H_ */