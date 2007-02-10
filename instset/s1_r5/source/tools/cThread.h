/*
 *  cThread.h
 *  Avida
 *
 *  Created by David on 2/18/06.
 *  Copyright 2006 Michigan State University. All rights reserved.
 *
 */

#ifndef cThread_h
#define cThread_h

#ifndef NULL
#define NULL 0
#endif

#include <pthread.h>

class cThread
{
protected:
  pthread_t m_thread;
  pthread_mutex_t m_mutex;
  bool m_running;

  virtual void Run() = 0;
  
  static void* EntryPoint(void* arg);

  cThread(const cThread&); // @not_implemented
  cThread& operator=(const cThread&); // @not_implemented

public:
  cThread() : m_running(false) { pthread_mutex_init(&m_mutex, NULL); }
  virtual ~cThread();
  
  int Start();
  void Stop();
  void Join();
};


#ifdef ENABLE_UNIT_TESTS
namespace nThread {
  /**
   * Run unit tests
   *
   * @param full Run full test suite; if false, just the fast tests.
   **/
  void UnitTests(bool full = false);
}
#endif  

#endif
