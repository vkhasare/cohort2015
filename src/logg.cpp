#include <glog/logging.h>
#include <gflags/gflags.h>
#include "logg.h"

extern unsigned int flag;

extern "C" void LOGGING_WARNING(char* string){
  LOG(WARNING) << string;
  return;
}

extern "C" void LOGGING_INFO(char* string){
  LOG(INFO) << string;
  return;
}

extern "C" void LOGGING_ERROR(char* string){
  LOG(ERROR) << string;
  return;
}

extern "C" void LOGGING_WARNING_IF(int cond,char* string){
	LOG_IF(WARNING, cond) << string;
  return;
}

extern "C" void LOGGING_INFO_IF(int cond,char* string){
  LOG_IF(INFO, cond) << string;
  return;
}

extern "C" void LOGGING_ERROR_IF(int cond,char* string){
	LOG_IF(ERROR, cond) << string;
  return;
}
																					
void initialize_logging(int argc, char * argv[])
{
  google::SetLogDestination(google::GLOG_INFO,"/tmp" );
	google::InitGoogleLogging(argv[0]);
	google::ParseCommandLineFlags(&argc, &argv, true);
	return;
}

void cease_logging(){
  google::ShutdownGoogleLogging();
}


