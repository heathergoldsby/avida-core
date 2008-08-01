/*
 *  cASFunction.h
 *  Avida
 *
 *  Created by David on 3/16/08.
 *  Copyright 2008 Michigan State University. All rights reserved.
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef cASFunction_h
#define cASFunction_h

#include "avida.h"
#include "AvidaScript.h"

#include "cString.h"


class cASFunction
{
public:
  class cParameter
  {
  private:
    union {
      bool m_bool;
      char m_char;
      int m_int;
      double m_float;
      cString* m_string;
    };
    
  public:
    cParameter() { ; }
    
    void Set(bool val) { m_bool = val; }
    void Set(char val) { m_char = val; }
    void Set(int val) { m_int = val; }
    void Set(double val) { m_float = val; }
    void Set(cString* val) { m_string = val; }
    void Set(const cString& val) { m_string = new cString(val); }
    
    template<typename T> inline T Get() const { Avida::Exit(AS_EXIT_INTERNAL_ERROR); return m_int; }
  };
  
  
protected:
  cString m_name;
  sASTypeInfo m_rtype;
  
  
public:
  cASFunction(const cString& name) : m_name(name) { ; }
  virtual ~cASFunction() { ; }
  
  const cString& GetName() const { return m_name; }

  virtual int GetArity() const = 0;
  const sASTypeInfo& GetReturnType() const { return m_rtype; }
  virtual const sASTypeInfo& GetArgumentType(int arg) const = 0;
  
  virtual cParameter Call(cParameter args[]) const = 0;
};


template<> inline bool cASFunction::cParameter::Get<bool>() const { return m_bool; }
template<> inline char cASFunction::cParameter::Get<char>() const { return m_char; }
template<> inline int cASFunction::cParameter::Get<int>() const { return m_int; }
template<> inline double cASFunction::cParameter::Get<double>() const { return m_float; }
template<> inline const cString& cASFunction::cParameter::Get<const cString&>() const { return *m_string; }
template<> inline const cString* cASFunction::cParameter::Get<const cString*>() const { return m_string; }
template<> inline cString* cASFunction::cParameter::Get<cString*>() const { return m_string; }


template<typename FunctionType> class tASFunction;


template<typename ReturnType, typename Arg1Type> 
class tASFunction<ReturnType (Arg1Type)> : public cASFunction
{
private:
  typedef ReturnType (*TrgtFunType)(Arg1Type);

  sASTypeInfo m_signature;
  TrgtFunType m_func;
  
  
public:
  tASFunction(TrgtFunType func, const cString& name) : cASFunction(name), m_func(func)
  {
    m_rtype = AvidaScript::TypeOf<ReturnType>();
    m_signature = AvidaScript::TypeOf<Arg1Type>();
  }
  
  int GetArity() const { return 1; }
  const sASTypeInfo& GetArgumentType(int arg) const { return m_signature; }
  
  cParameter Call(cParameter args[]) const
  {
    cParameter rvalue;
    rvalue.Set((*m_func)(args[0].Get<Arg1Type>()));
    return rvalue;
  }
};
                  
                  
template<typename Arg1Type> 
class tASFunction<void (Arg1Type)> : public cASFunction
{
private:
  typedef void (*TrgtFunType)(Arg1Type);
  
  sASTypeInfo m_signature;
  TrgtFunType m_func;
  
  
public:
  tASFunction(TrgtFunType func, const cString& name) : cASFunction(name), m_func(func)
  {
    m_rtype = AvidaScript::TypeOf<void>();
    m_signature = AvidaScript::TypeOf<Arg1Type>();
  }
  
  int GetArity() const { return 1; }
  const sASTypeInfo& GetArgumentType(int arg) const { return m_signature; }
  
  cParameter Call(cParameter args[]) const
  {
    (*m_func)(args[0].Get<Arg1Type>());
    
    return cParameter();
  }
};



#endif
