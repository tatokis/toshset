
//#include <sthead.hh>
#include <cdsMath.hh>
#include <cstdlib> // for std::abs

namespace CDS {

//realtype ipow(const realtype &x,
//		      int       i)
///* return the ith integer power of x
//   7/12/93 - now works for negative integers too*/
//{
// realtype temp = 1;
// for (int cnt=0 ; cnt<abs(i) ; cnt++) temp *= x;
//
// if (i<0 && temp != 0.0) temp = 1.0 / temp;
//
// return temp;
//} /*ipow*/


template<class T> T 
ipow(const T&  x,
	   int i)
/* return the ith integer power of x
   - works for negative integers too*/
{
 T temp = 1;
 for (int cnt=0 ; cnt<std::abs(i) ; cnt++) temp *= x;

 if (i<0 && temp != 0.0) temp = 1.0 / temp;

 return temp;
} /* pow */

} /* namespace CDS */
