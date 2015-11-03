#include "logg.h"

int flag = 1;

int main(int argc, char* argv[]){
    initialize_logging(argc, argv); 
    LOGGING_WARNING_IF(1, "Conditional warning string");
    LOGGING_WARNING("Warning String");
    cease_logging();
    return 0;
}
