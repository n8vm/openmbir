/* ============================================================================
 * Copyright (c) 2011, Michael A. Jackson (BlueQuartz Software)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * Neither the name of Michael A. Jackson nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#ifndef TOMOARRAY_HPP_
#define TOMOARRAY_HPP_


#include <stdio.h>
#include <iostream>
#include <string>

#include <boost/shared_ptr.hpp>


template<typename T, typename Ptr, int size>
class TomoArray
{
  public:
    typedef TomoArray<T, Ptr, size> Self;
    typedef boost::shared_ptr<Self> Pointer;
    typedef boost::shared_ptr<const Self> ConstPointer;
    typedef boost::weak_ptr<TomoArray<T, Ptr, size> > WeakPointer;
    typedef boost::weak_ptr<TomoArray<T, Ptr, size> > ConstWeakPointer;
    static Pointer NullPointer(void)
    {
      return Pointer(static_cast<TomoArray<T, Ptr, size>*>(0));
    }

    static Pointer New(size_t* dims)
    {
      Pointer sharedPtr(new TomoArray<T, Ptr, size>(dims));
      return sharedPtr;
    }

    virtual ~TomoArray()
    {
      if (m_Name.empty() == false)
      {
        std::cout << "Deallocating TomoArray " << m_Name << std::endl;
      }
      if (size == 1)
      {
        free(d);
      }
      else {
        deallocate((T*)d,size);
      }
    }

    void setName(const std::string &name) { m_Name = name;}
    Ptr getPointer() { return d; }
    size_t* getDims() {return m_Dims; }
    int getNDims() { return m_NDims; }
    int getTypeSize() { return sizeof(T); }

    Ptr d;

  protected:
    TomoArray(size_t* dims) :
      m_Dims(dims)
    {
      d = reinterpret_cast<Ptr>(allocate(sizeof(T), size, m_Dims));
    }

  private:
    size_t* m_Dims;
    int  m_NDims;
    std::string m_Name;


    T* allocate(size_t s, unsigned int d, size_t* d1)
    {
     // va_list ap;             /* varargs list traverser */
      size_t max,                /* size of array to be declared */
      *q;                     /* pointer to dimension list */
      char **r,               /* pointer to beginning of the array of the
                               * pointers for a dimension */
      **s1, *t, *tree;        /* base pointer to beginning of first array */
      size_t i, j;               /* loop counters */
     // int *d1;                /* dimension list */

     // va_start(ap,d);
     // d1 = (int *) mget_spc(d,sizeof(int));

//      for(i=0;i<d;i++)
//        d1[i] = va_arg(ap,int);

      r = &tree;
      q = d1;                /* first dimension */
      max = 1;
      for (i = 0; i < d - 1; i++, q++) {      /* for each of the dimensions
                                               * but the last */
        max *= (*q);
//        r[0]=(char *)mget_spc(max,sizeof(char**));
        r[0]=(char*)malloc((size_t)(max*sizeof(char**)));
     //   printf("max: %d   sizeof(char **): %d  r: %p r[0]: 0x%08x \n", max , sizeof(char **), r, r[0]);
        r = (char **) r[0];     /* step through to beginning of next
                                 * dimension array */
      }
      max *= s * (*q);        /* grab actual array memory */
      //r[0] = (char*)mget_spc(max,sizeof(char));
      r[0]=(char*)malloc((size_t)(max*sizeof(char)));
     // printf("max: %d   sizeof(char): %d  r: %p r[0]: 0x%08x \n", max , sizeof(char), r, r[0]);
      /*
       * r is now set to point to the beginning of each array so that we can
       * use it to scan down each array rather than having to go across and
       * then down
       */
      r = (char **) tree;     /* back to the beginning of list of arrays */
      q = d1;                 /* back to the first dimension */
      max = 1;
      for (i = 0; i < d - 2; i++, q++) {      /* we deal with the last
                                               * array of pointers later on */
        max *= (*q);    /* number of elements in this dimension */
        for (j=1, s1=r+1, t=r[0]; j<max; j++) { /* scans down array for
                                                 * first and subsequent
                                                 * elements */

        /*  modify each of the pointers so that it points to
         * the correct position (sub-array) of the next
         * dimension array. s1 is the current position in the
         * current array. t is the current position in the
         * next array. t is incremented before s1 is, but it
         * starts off one behind. *(q+1) is the dimension of
         * the next array. */

          *s1 = (t += sizeof (char **) * *(q + 1));
          s1++;
        }
        r = (char **) r[0];     /* step through to begining of next
                                 * dimension array */
      }
      max *= (*q);              /* max is total number of elements in the
                                 * last pointer array */

      /* same as previous loop, but different size factor */
      for (j = 1, s1 = r + 1, t = r[0]; j < max; j++)
        *s1++ = (t += s * *(q + 1));

//      va_end(ap);
//      free((void *)d1);
      return (T*)tree;              /* return base pointer */
    }
    /*
     * multifree releases all memory that we have already declared analogous to
     * free() when using malloc()
     */

    void deallocate(T* r, int d)
    {
      void** p;
      void* next = NULL;
      int i;

      for (p = (void**)r, i = 0; i < d; p = (void**)next, i++)
      {
        if(p != NULL)
        {
          next = *p;
         // printf("p: %p  Value of P: 0x%08x  i:  %d   Value of Next: 0x%08x \n", p, *p, i, next);
          free((void*)p);
        }
      }
    }


    TomoArray(const TomoArray&); // Copy Constructor Not Implemented
    void operator=(const TomoArray&); // Operator '=' Not Implemented

};


#endif /* TOMOARRAY_HPP_ */