
#include "cdsSStream.hh"
#include "cdsMath.hh"

template<class CHAR>
CDSStringStreamBuf<CHAR>::CDSStringStreamBuf()
{
} /* constructor */

template<class CHAR>
CDSStringStreamBuf<CHAR>::CDSStringStreamBuf(const CDSString<CHAR>& s) :
  str_(s)
{
 setup(s,CDSString<CHAR>());
} /* constructor */

template<class CHAR>
CDSStringStreamBuf<CHAR>::~CDSStringStreamBuf()
{
} /* destructor */

template<class CHAR>
void
CDSStringStreamBuf<CHAR>::setup(CDSString<CHAR> get, 
				CDSString<CHAR> put)
{
 if ( put.length() ) {
   char* putCharPtr = put.charPtr();
   setp(putCharPtr, putCharPtr+put.length());
 }

 if ( get.length() ) {
   char* getCharPtr = get.charPtr();
   setg(getCharPtr, getCharPtr, getCharPtr+get.length());
 }
}

template<class CHAR>
int
CDSStringStreamBuf<CHAR>::underflow()
{
 // cout << "underflow: " << str_ << ' ' << endl;

// cout << "underflow: " 
//	<< gptr() << ' ' << egptr() << ' ' << pptr() << endl;

 if (gptr() == egptr() && pptr() && pptr() > egptr())
   setg(eback(), gptr(), pptr());
 
 if (gptr() != egptr())
   return (unsigned char) *gptr();
 else
   return EOF;
} /* underflow */

template<class CHAR>
int
CDSStringStreamBuf<CHAR>::pbackfail(int c)
//	    Is called when eback() equals gptr() and an attempt has been made
//	    to putback c.  If this situation can be dealt with (e.g., by
//	    repositioning an external file), pbackfail() should return c;
//	    otherwise it should return EOF.
{
 cout << "NOT TESTED: pbackfail: " << str_ << ' ' << c << endl;

 if (gptr() != eback()) {
   // clearly, this fails
   //if (c == EOF) {
   //  gbump(-1);
   //  return c; 
   //}
   //else 
   if (c == gptr()[-1]) { 
     gbump(-1);
     return c;
   }
   //else if (!_M_constant) {
   //  gbump(-1);
   //  *gptr() = c;
   //  return c;
   //}
 }

  return EOF;
}

template<class CHAR>
int 
CDSStringStreamBuf<CHAR>::overflow(int c)
{
 // cout << "overflow: " << str_ << ' ' << c << endl;
// if (c == traits_type::eof())
//   return traits_type::not_eof(c);
 
 if ( pptr() == epptr() ) {
   // expand the buffer
   ptrdiff_t old_size = epptr() - pbase();
   ptrdiff_t new_size = CDS::max(2 * old_size, ptrdiff_t(1));

   str_.resize(new_size);
 
   ptrdiff_t old_get_offset = gptr() - eback();

 
   CHAR* charPtr = str_.charPtr();
   setp(charPtr, charPtr + new_size);
   pbump(old_size);
 
   setg(charPtr, charPtr + old_get_offset, 
	charPtr + CDS::max(old_get_offset, old_size));
 }
 
 
 if ( pptr() != epptr() ) {
   *pptr() = c;
   pbump(1);
   return c;
 } else
   return EOF;
} /* overflow */

template<class CHAR>
streambuf*
CDSStringStreamBuf<CHAR>::setbuf(char* p,
				 int   len)
{
 cout << "NOT TESTED: setbuf: " << str_ << ' ' << p << endl;
 if ( p && len )
   str_ = CDSString<CHAR>(p,len);
 return this;
} /* setbuf */

//template<class CHAR>
//streampos
//CDSStringStreamBuf<CHAR>::seekoff(streamoff     off,
//				  ios::seek_dir dir, 
//				  int           mode)
//{
// cout << "NOT TESED: seekoff: " << str_ << ' ' << off << endl;
// bool do_get = false;
// bool do_put = false;
// 
// if ((mode & (ios::in | ios::out)) == (ios::in | ios::out) &&
//     (dir == ios::beg || dir == ios::end))
//   do_get = do_put = true;
// else if (mode & ios::in)
//   do_get = true;
// else if (mode & ios::out)
//   do_put = true;
//
// // !gptr() is here because, according to D.7.1 paragraph 4, the seekable
// // area is undefined if there is no get area.
// if ((!do_get && !do_put) || (do_put && !pptr()) || !gptr())
//   return streampos(streamoff(-1));
//
// char* seeklow  = eback();
// char* seekhigh = epptr() ? epptr() : egptr();
//
// streamoff newoff;
// switch(dir) {
//   case ios::beg:
//     newoff = 0;
//     break;
//   case ios::end:
//     newoff = seekhigh - seeklow;
//     break;
//   case ios::cur:
//     newoff = do_put ? pptr() - seeklow : gptr() - seeklow;
//     break;
//   default:
//     return streampos(streamoff(-1));
// }
//
// off += newoff;
// if (off < 0 || off > seekhigh - seeklow)
//   return streampos(streamoff(-1));
//
// if (do_put) {
//   if (seeklow + off < pbase()) {
//     setp(seeklow, epptr());
//     pbump(off);
//   } else {
//     setp(pbase(), epptr());
//     pbump(off - (pbase() - seeklow));
//   }
// }
// if (do_get) {
//   if (off <= egptr() - seeklow)
//     setg(seeklow, seeklow + off, egptr());
//   else if (off <= pptr() - seeklow)
//     setg(seeklow, seeklow + off, pptr());
//   else
//     setg(seeklow, seeklow + off, epptr());
// }
//
// return streampos(newoff);
//} /* seekoff */
//
//template<class CHAR>
//streampos
//CDSStringStreamBuf<CHAR>::seekpos(streampos pos, 
//				  int       mode)
//{
// return seekoff(pos - streampos(streamoff(0)), ios::beg, mode);
//} /* seekpos */

//
// CDSOStringStream defs
//
//

template<class CHAR>
CDSOStringStream<CHAR>::CDSOStringStream()
  : ios(), ostream(&buf), buf()
{
 ios::init(&buf);
}

template<class CHAR>
CDSOStringStream<CHAR>::~CDSOStringStream()
{
}


//
// CDSIStringStream defs
//
//

template<class CHAR>
CDSIStringStream<CHAR>::CDSIStringStream(const CDSString<CHAR>& s)
  : istream(&buf), ios(), buf(s)
{
 ios::init(&buf);
}

template<class CHAR>
CDSIStringStream<CHAR>::CDSIStringStream(const CHAR* s)
  : ios(), istream(&buf), buf(s)
{
 ios::init(&buf);
}

template<class CHAR>
CDSIStringStream<CHAR>::~CDSIStringStream()
{
}


//
// CDSStringStream defs
//
//

template<class CHAR>
CDSStringStream<CHAR>::CDSStringStream()
  : iostream(&buf), ios(), buf()
{
 ios::init(&buf);
}

template<class CHAR>
CDSStringStream<CHAR>::~CDSStringStream()
{
}


#ifdef TESTING
#include <iomanip.h>

template<class CHAR>
int
CDSStringStream<CHAR>::test()
{
 int exit=0;
 cout << "testing CDSSStream...";

 {
   StringStream str;
   str << "hello";
   str << ends;
   
   if (str.str() != "hello") {
     cerr << "CDSStringStream: failure: " << str.str() << " != hello" << endl;
     exit=1;
   }
 }

 {
   OStringStream str;
   str << "hello2";
   str << ends;

   if (str.str() != "hello2") {
     cerr << "CDSOStringStream: failure: " << str.str() << " != hello2" 
	  << endl;
     exit=1;
   }
 }

 {
   IStringStream str( "42 hello" );
   int num;
   String s;
   str >> num >> s;
   if ( num != 42 || s != "hello" ) {
     cerr << "CDSIStringSream: failure: " << num << ' ' << s << endl;
     exit=1;
   }
 }

 cout << (exit?"failed":"ok") << endl;
 return exit;
} /* test */

#endif /* TESTING */
