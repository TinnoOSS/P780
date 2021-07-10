/*
 * Copyright (c) 2014-2016, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#if !defined(TRACE_JOURNEY_H) || defined(TRACE_HEADER_MULTI_READ)
#define TRACE_JOURNEY_H

#ifndef TRACE_JOURNEY_SYSTEM
#define TRACE_JOURNEY_SYSTEM journey
#endif

#undef TRACE_SYSTEM
#define TRACE_SYSTEM TRACE_JOURNEY_SYSTEM
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE journey_trace

#define __JOURNEY_TRACE_MAKE_WRITE2(prefix,subsys) prefix##subsys##_tracing_mark_write
#define JOURNEY_TRACE_MAKE_WRITE2(prefix,subsys) __JOURNEY_TRACE_MAKE_WRITE2(prefix,subsys)
#define JOURNEY_TRACE_MAKE_WRITE(subsys) JOURNEY_TRACE_MAKE_WRITE2(,subsys)


#include <linux/tracepoint.h>
// found the event name in /external/perfetto/src/trace_processor/ftrace_descriptors.cc
TRACE_EVENT(JOURNEY_TRACE_MAKE_WRITE(TRACE_SYSTEM),
	TP_PROTO(char trace_type, const struct task_struct *task,
		 const char *name, int value),
	TP_ARGS(trace_type, task, name, value),
	TP_STRUCT__entry(
			__field(char, trace_type)
			__field(int, pid)
			__string(trace_name, name)
			__field(int, value)
	),
	TP_fast_assign(
			__entry->trace_type = trace_type;
			__entry->pid = task ? task->tgid : 0;
			__assign_str(trace_name, name);
			__entry->value = value;
	),
	TP_printk("%c|%d|%s|%d", __entry->trace_type,
		__entry->pid, __get_str(trace_name), __entry->value)
);

TRACE_EVENT_FLAGS(JOURNEY_TRACE_MAKE_WRITE(TRACE_SYSTEM),TRACE_EVENT_FL_JOURNEY_ATRACE)

#define journey_atrace JOURNEY_TRACE_MAKE_WRITE2(trace_,TRACE_SYSTEM)
#define JOURNEY_ATRACE_GLOBAL_INT(name, value) journey_atrace('C', NULL, name, value)
#define JOURNEY_ATRACE_INT(name, value) journey_atrace('C', current, name, value)
#define JOURNEY_ATRACE_END(name) journey_atrace('E', current, name, __LINE__)
#define JOURNEY_ATRACE_BEGIN(name) journey_atrace('B', current, name, __LINE__)
#define JOURNEY_ATRACE_FUNC() JOURNEY_ATRACE_BEGIN(__func__)
#define JOURNEY_ATRACE_FUNC_END() JOURNEY_ATRACE_END(__func__)
#endif /* if !defined(TRACE_JOURNEY_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
