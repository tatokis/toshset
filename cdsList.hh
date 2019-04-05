/*
   CDSList.h
   headers for my own List template class
   9/1/95 - CDS
*/

#ifndef __cdslist_hh__
#define __cdslist_hh__ 1

/* ostream is a typedef in the new standard, a class in the old, so forward
 * declarations are painful
 */
#include <iostream> 
using namespace std;

template<class T,int DefaultSize=10> 
class CDSList {
  int size_;  //number of elements used
  int asize;  //numer of elements allocated
  int blockSize;  //size for next allocation block
  T* data;
  //  enum { DefaultListSize = 100 };
public:

  CDSList (int s=0, int a=DefaultSize); 
  
  ~CDSList ();
  
  CDSList (const  CDSList<T,DefaultSize>&); //copy constructor

  CDSList &resize(const int s);

  CDSList  &operator= (const  CDSList<T,DefaultSize>&);
  const T  &operator[] (const int &index) const;
        T  &operator[] (const int &index);
  //  operator T*() { return data; } //convert to array

  const T  &append(const T& member);
  bool     contains(const T& member);
  void     remove(const int i);
  int      getIndex(const T& member);
  void     setBlockSize(const int);
  //  int      size() {return size_;}
  int      size() const {return size_;}
  //  template<class T2> 
//  friend ostream& operator<<(ostream& os, 
//			       const CDSList<T,DefaultSize>& v);

#ifdef TESTING
  static int test();
#endif
};

template <class T,int S> 
ostream& 
operator<<(ostream& os, const CDSList<T,S>& v);

//constructors

template<class T,int S>
CDSList<T,S>::CDSList(const int s,
		      const int a) :
  size_(s), asize(0), blockSize(a)
{
 while (size_ > asize) {
   asize += blockSize;
 }

 data = new T[asize];
} /* CDSList::CDSList */

template<class T,int S>
CDSList<T,S>::~CDSList()
{
 delete [] data;
}

template<class T,int S>
inline T&
CDSList<T,S>::operator[](const int &index)  //non-const version
{
#ifdef CHECKRANGE
 if (index+1>size_ || index<0) {
   cerr << "CDSList::operator[]: index out of range (" << index << ")\n";
   abort();
 }
#endif
 return (data[index]);
}

template<class T,int S>
inline const T& 
CDSList<T,S>::operator[](const int &index) const  //const version
{
#ifdef CHECKRANGE
 if (index+1>size_ || index<0) {
   cerr << "CDSList::operator[]: index out of range (" << index << ")\n";
   abort();
 }
#endif
 return (data[index]);
}

template<class T, int S>
const T& 
CDSList<T,S>::append(const T &member)
{
 resize(size_+1);
 data[size_-1] = member;
 return member;
}
 


#endif /*__cdslist_hh__*/
