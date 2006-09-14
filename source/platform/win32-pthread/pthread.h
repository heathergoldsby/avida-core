/*
 *  pthread.h
 *  
 *
 *  Created by David on 3/28/06.
 *  Copyright 2006 Michigan State University. All rights reserved.
 *
 */

#ifndef pthread_h
#define pthread_h

// @DMB - This is a hack to disable pthreads on Win32 until I can integrate
//        the win32-pthreads package

// Just define away mutex support
#define pthread_mutex_t int
#define pthread_mutex_init(x)
#define pthread_mutex_lock(x)
#define pthread_mutex_unlock(x)
#define pthread_mutex_destroy(x)

// Just define away condition variable support
#define pthread_cond_t int
#define pthread_cond_init(x)
#define pthread_cond_wait(x, y)
#define pthread_cond_signal(x)
#define pthread_cond_broadcast(x)
#define pthread_cond_destroy(x)

// Just define away rwlock support
#define pthread_rwlock_t int
#define pthread_rwlock_init(x, y)
#define pthread_rwlock_rdlock(x)
#define pthread_rwlock_wrlock(x)
#define pthread_rwlock_unlock(x)
#define pthread_rwlock_destroy(x)

// Define away pthread support
#define pthread_t int
#define pthread_exit(x)

#define WIN32_PTHREAD_HACK


#endif
