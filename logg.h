#include <glog/logging.h>

void initialize_logging(int argc, char * argv[]);

#define LOGGING_WARNING(string) \
	if (flag)                   	\
		VLOG(1) << string;			\
	LOG(WARNING) << string

#define LOGGING_INFO(string) \
	if (flag)                     	\
		VLOG(0) << string;     		\
	LOG(INFO) << string

#define LOGGING_ERROR(string) \
	if (flag)                      	\
		VLOG(2) << string;     		\
	LOG(ERROR) << string

#define LOGGING_WARNING_IF(cond,string) \
	if(flag) 							\
		VLOG_IF(1, cond) << string;		\
	LOG_IF(WARNING, cond) << string

#define LOGGING_INFO_IF(cond,string) \
	if(flag)							\
		VLOG_IF(0, cond) << string;		\
	LOG_IF(INFO, cond) << string

#define LOGGING_ERROR_IF(cond,string) \
	if(flag)							\
		VLOG_IF(2, cond) << string;		\
	LOG_IF(ERROR, cond) << string
																					
void initialize_logging(int argc, char * argv[])
{
	google::SetLogDestination(google::GLOG_INFO,"/root/server" );
	google::InitGoogleLogging(argv[0]);
	google::ParseCommandLineFlags(&argc, &argv, true);

	return;
}
