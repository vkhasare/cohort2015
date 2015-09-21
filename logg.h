#include <glog/logging.h>

void initialize_logging(int argc, char * argv[]);

#define TRACE(severity,string) \
	LOG(severity) << string

#define TRACE_IF(severity,cond,string) \
	LOG_IF(severity, cond) << string

#define DTRACE(severity,string) \
	DLOG(severity) << string

#define VTRACE(severity,string) \
	VLOG(severity) << string

void initialize_logging(int argc, char * argv[])
{
	google::SetLogDestination(google::GLOG_INFO,"/root/server" );
	google::InitGoogleLogging(argv[0]);
	google::ParseCommandLineFlags(&argc, &argv, true);

	return;
}
