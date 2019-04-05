
#include "cdsList.hh"

template<class T, int S> 
CDSList<T,S>& 
CDSList<T,S>::resize(const int nsize)
{
 if ( nsize>asize ) {
   while ( nsize>asize ) {
     asize += (blockSize?blockSize:1);
   }

   T *ndata = new T[asize];
   for (int i=0 ; i<size_ ; i++) ndata[i] = data[i];

   delete [] data;
   data = ndata;
 }

 size_ = nsize;

 return *this;
} /*resize*/

 
template<class T, int S> 
CDSList<T,S>::CDSList(const CDSList<T,S> &l)
  //
  // copy constructor
  //
{
 size_     = l.size_;
 asize     = l.asize;
 blockSize = l.blockSize;

 data = new T[l.asize];
 for (int i=0 ; i<size_ ; i++)
   data[i] = l.data[i];
} /* CDSList(const CDSList&) */

template<class T, int S> 
CDSList<T,S>& 
CDSList<T,S>::operator=(const CDSList<T,S> &l)
  //
  // assignment of list to list
  //
{
 if (&l==this)  //self-assignment
   return *this;
 
 if (asize != l.asize) {
   delete [] data;
   data = new T[l.asize];
 }

 size_     = l.size_;
 asize     = l.asize;
 blockSize = l.blockSize;

 for (int i=0 ; i<size_ ; i++)
   data[i] = l.data[i];
 return *this;
}

template<class T, int S> 
int
CDSList<T,S>::getIndex(const T &member)
{
 for (int i=0 ; i<size_ ; i++)
   if ( data[i] == member )
     return i;

 return -1;
}
 

template<class T, int S> 
bool
CDSList<T,S>::contains(const T &member)
{
 if ( getIndex(member) >= 0 )
   return 1;
 else
   return 0;
}
 

template<class T, int S>
void 
CDSList<T,S>::remove(const int i)
  //
  // remove an element of the list
  //
{
#ifdef CHECKRANGE
 if (i+1>size_ || i<0) {
   cerr << "CDSList::remove: index out of range:" << i << "\n";
   abort();
 }
#endif
 for (int j=i+1 ; j<size_ ; j++)
   data[j-1] = data[j];
 size_--;
}
 

template<class T, int S>
void 
CDSList<T,S>::setBlockSize(const int nsize)
{
 blockSize = nsize;
}

template <class T, int S> 
ostream& 
operator<<(ostream& os, const CDSList<T,S>& v)
{
 os << '{';
 for (int i=0 ; i<v.size() ; i++) {
   os << ' ' << v[i];
   if ( i<v.size()-1 ) os << ',';
 }
 os << " }";
 return os;
}

#ifdef TESTING

#include <stdlib.h>
#include <string.h>
#include <strstream.h>

template<class T, int S>
int
CDSList<T,S>::test()
{
 int exit=0;
 cout << "testing CDSList...";

 CDSList<int> l;
 CDSList<int> l2(1,2);
 CDSList<int> l3 = l2;

 l.append(1);
 
 if (l[0] != 1) {
   cerr << "index failed: " << l[0] << " != 1\n";
   exit=1;
 }

 if (l3.size()!=1 || l3.asize!=2) {
   cerr << "internal error 1\n";
   exit=1;
 }

 l3.resize(3);
 if (l3.size()!=3 || l3.asize!=4) {
   cerr << "internal error 2\n";
   exit=1;
 }
 
 l.append(4);
 if ( !l.contains(4) ) {
   cerr << "contains error\n";
   exit=1;
 }

 if ( l.getIndex(4)!=1 ) {
   cerr << "getIndex error\n";
   exit=1;
 }

 ostrstream os; os << l;
 if ( strcmp(os.str(),"{ 1, 4 }") ) {
   cerr << "operator<< error: " << os.str() << " != { 1, 4 }\n";
   exit=1;
 }

 cout << (exit?"failed":"ok") << endl;
 return exit;
} /* test */

#endif /* TESTING */
