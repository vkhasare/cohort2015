#include <glog/logging.h>
#include <gflags/gflags.h>
#include "logg.h"

extern unsigned int flag;

extern "C" void logging_warning(char* string, char* fname, int lno){
  google::LogMessage(fname, lno, google::GLOG_WARNING).stream() << string;
  return;
}

extern "C" void logging_info(char* string, char* fname, int lno){
  google::LogMessage(fname, lno).stream() << string;
  return;
}

extern "C" void logging_error(char* string, char* fname, int lno){
  google::LogMessage(fname, lno, google::GLOG_ERROR).stream() << string;
  return;
}

extern "C" void logging_warning_if(int cond,char* string, char* fname, int lno){
  !(cond) ? (void) 0: google::LogMessageVoidify() & 
      google::LogMessage(fname, lno, google::GLOG_WARNING).stream() << string;

  return;
}

extern "C" void logging_info_if(int cond,char* string, char* fname, int lno){
    !(cond) ? (void) 0: google::LogMessageVoidify() &
        google::LogMessage(fname, lno).stream() << string;
  return;
}

extern "C" void logging_error_if(int cond,char* string, char* fname, int lno){
    !(cond) ? (void) 0: google::LogMessageVoidify() &
        google::LogMessage(fname, lno, google::GLOG_ERROR).stream() << string;
  return;
}
																					
void initialize_logging(int argc, char * argv[])
{
  //google::SetLogDestination(google::GLOG_INFO,"/tmp" );
  FLAGS_logbufsecs = 0;
	google::InitGoogleLogging(argv[0]);
	google::ParseCommandLineFlags(&argc, &argv, true);
	return;
}

void cease_logging(){
  google::ShutdownGoogleLogging();
}


