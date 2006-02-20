// -*- mode:C++; tab-width:4; c-basic-offset:2; indent-tabs-mode:t -*- 
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <string>

#include "LogType.h"
#include "Logger.h"

#include <iostream>
#include "Clock.h"

#include "config.h"

#include <sys/stat.h>
#include <sys/types.h>


// per-process lock.  lame, but this way I protect LogType too!
Mutex logger_lock;

Logger::Logger(string fn, LogType *type)
{
  filename = "log/";
  if (g_conf.log_name) {
	filename += g_conf.log_name;
	::mkdir( filename.c_str(), 0755 );   // make sure dir exists
	filename += "/";
  }
  filename += fn;
  //cout << "log " << filename << endl;
  interval = g_conf.log_interval;
  start = g_clock.now();  // time 0!
  last_logged = 0;
  wrote_header = -1;
  open = false;
  this->type = type;
  wrote_header_last = 0;

  flush(false);
}

Logger::~Logger()
{
  flush(true);
  out.close();
}

long Logger::inc(const char *key, long v)
{
  if (!g_conf.log) return 0;
  logger_lock.Lock();
  int i = type->lookup_key(key);
  if (i < 0) i = type->add_inc(key);
  flush();
  vals[i] += v;
  long r = vals[i];
  logger_lock.Unlock();
  return r;
}

double Logger::finc(const char *key, double v)
{
  if (!g_conf.log) return 0;
  logger_lock.Lock();
  int i = type->lookup_key(key);
  if (i < 0) i = type->add_inc(key);
  flush();
  fvals[i] += v;
  double r = fvals[i];
  logger_lock.Unlock();
  return r;
}

long Logger::set(const char *key, long v)
{
  if (!g_conf.log) return 0;
  logger_lock.Lock();
  int i = type->lookup_key(key);
  if (i < 0) i = type->add_set(key);
  flush();
  long r = vals[i] = v;
  logger_lock.Unlock();
  return r;
}


double Logger::fset(const char *key, double v)
{
  if (!g_conf.log) return 0;
  logger_lock.Lock();
  int i = type->lookup_key(key);
  if (i < 0) i = type->add_set(key);
  flush();
  double r = fvals[i] = v;
  logger_lock.Unlock();
  return r;
}

long Logger::get(const char* key)
{
  if (!g_conf.log) return 0;
  logger_lock.Lock();
  int i = type->lookup_key(key);
  long r = 0;
  if (i >= 0)
	r = vals[i];
  logger_lock.Unlock();
  return r;
}

void Logger::flush(bool force) 
{
  if (!g_conf.log) return;
  logger_lock.Lock();

  while (type->keys.size() > vals.size())
	vals.push_back(0);
  while (type->keys.size() > fvals.size())
	fvals.push_back(0);

  if (!open) {
	out.open(filename.c_str(), ofstream::out);
	open = true;
	//cout << "opening log file " << filename << endl;
  }

  utime_t fromstart = g_clock.recent_now();
  fromstart -= start;

  while (force ||
		 fromstart.sec() - last_logged >= interval) {
	last_logged += interval;
	force = false;

	//cout << "logger " << this << " advancing from " << last_logged << " now " << now << endl;

	if (!open) {
	  out.open(filename.c_str(), ofstream::out);
	  open = true;
	  //cout << "opening log file " << filename << endl;
	}

	// header?
	wrote_header_last++;
	if (wrote_header != type->version ||
		wrote_header_last > 10) {
	  out << "#" << type->keymap.size();
	  for (unsigned i=0; i<type->keys.size(); i++) 
		out << "\t" << type->keys[i];
	  out << endl;  //out << "\t (" << type->keymap.size() << ")" << endl;
	  wrote_header = type->version;
	  wrote_header_last = 0;
	}

	// write line to log
	out << last_logged;
	for (unsigned i=0; i<type->keys.size(); i++) {
	  if (fvals[i] > 0 && vals[i] == 0)
		out << "\t" << fvals[i];
	  else
		out << "\t" << vals[i];
	}
	out << endl;

	// reset the counters
	for (unsigned i=0; i<type->keys.size(); i++) {
	  if (type->inc_keys.count(i)) {
		this->vals[i] = 0;
		this->fvals[i] = 0;
	  }
	}
  }

  logger_lock.Unlock();
}




