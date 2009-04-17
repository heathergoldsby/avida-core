#include "cOrganism.h"
#include "cMerit.h"
#include "cMutation.h"
#include "cIntSum.h"
#include "cHardwareCPU_Thread.h"

#include "tArray.h"

#include <assert.h>
#include <stdexcept>

typedef cOrganism * pOrganism;
typedef cMutation * pMutation;

template <class T> void tArray_rangecheck(tArray<T> &array, int i){
  if (i < 0 || array.GetSize() <= i) throw std::out_of_range("Index out of range.");
}

template <class T> int tArray_class_T_len(tArray<T> &array) { return array.GetSize(); }
template <class T> T& tArray_class_T_getitem(tArray<T> &array, int i) {
  tArray_rangecheck<>(array, i);
  return array[i];
}
template <class T> void tArray_class_T_setitem(tArray<T> &array, int i, T &item) {
  tArray_rangecheck<>(array, i);
  array[i] = item;
}

template <typename T> T tArray_fundamental_T_getitem(tArray<T> &array, int i) {
  tArray_rangecheck<>(array, i);
  return array[i];
}
