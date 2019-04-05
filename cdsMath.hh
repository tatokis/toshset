
#ifndef __cdsmath_hh__
#define __cdsmath_hh__ 1

#include <math.h>

namespace CDS {

#ifndef PI
#     define PI 3.14159265358979323846
#endif

template<class T> T
ipow(const T&,int);

template<class T> inline
T sq(const T t)
{
 return t*t;
}

template<class T> inline
T max(T a, T b)
{
 return (a>b?a:b);
} /* max */

template<class T> inline
T min(T a, T b)
{
 return (a<b?a:b);
} /* min */

template<class T> inline
int sign(T x)
{
 return ((x<0)?-1:1);
}

//
// conversion factor between radians and degrees
//
const double RAD2DEG = 180. / PI;

} /* namespace CDS */

#endif /*__cdsmath_hh__*/
