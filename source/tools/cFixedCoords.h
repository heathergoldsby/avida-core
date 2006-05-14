/*
 *  cFixedCoords.h
 *  Avida
 *
 *  Created by David on 12/7/05.
 *  Copyright 2005-2006 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology
 *
 */

#ifndef cFixedCoords_h
#define cFixedCoords_h

#ifndef cUInt_h
#include "cUInt.h"
#endif

/**
 * Class used by @ref cBlockStruct.
 *
 * It handles co-ordinates in terms of block number
 * and offset.
 **/

class cFixedCoords {
private:
  int block_num;
  int offset;
public:
  inline cFixedCoords() {
    block_num = 0;
    offset = 0;
  }
  inline cFixedCoords(int in_block, int in_offset) {
    block_num = in_block;
    offset = in_offset;
  }

  inline int GetBlockNum() const { return block_num; }
  inline int GetOffset() const { return offset; }

  inline cUInt AsCUInt(int in_fixed_size) const {
    cUInt temp;
    temp = block_num * in_fixed_size + offset;
    return temp;
  }

  inline void operator()(int in_block, int in_offset) {
    block_num = in_block;
    offset = in_offset;
  }
  inline void operator=(const cFixedCoords & in_coords) {
    block_num = in_coords.GetBlockNum();
    offset = in_coords.GetOffset();
  }
  inline int operator<(const cFixedCoords & in_coords) const {
    return ((block_num < in_coords.block_num) ||
	    (block_num == in_coords.block_num && offset < in_coords.offset));
  }
  inline int operator<=(const cFixedCoords & in_coords) const {
    return ((block_num < in_coords.block_num) ||
	    (block_num == in_coords.block_num && offset <= in_coords.offset));
  }
  inline int operator>(const cFixedCoords & in_coords) const {
    return !operator<=(in_coords);
  }
  inline int operator>=(const cFixedCoords & in_coords) const {
    return !operator<(in_coords);
  }
  inline int operator==(const cFixedCoords & in_coords) const {
    return (block_num == in_coords.GetBlockNum() &&
            offset == in_coords.GetOffset());
  }

  inline void Add(cFixedCoords & other_coord, int fixed_size) {
    block_num += other_coord.GetBlockNum();
    offset += other_coord.GetOffset();
    block_num += offset / fixed_size;
    offset %= fixed_size;
  }
  inline void Add(int in_block, int in_offset, int fixed_size) {
    block_num += in_block;
    offset += in_offset;
    block_num += offset / fixed_size;
    offset %= fixed_size;
  }

  /**   
   * Serialize to and from archive.
   **/  
  template<class Archive>
  void serialize(Archive & a, const unsigned int version){
    a.ArkvObj("block_num", block_num);
    a.ArkvObj("offset", offset);
  }   
};


#ifdef ENABLE_UNIT_TESTS
namespace nFixedCoords {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif
