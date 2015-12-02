#ifdef __cplusplus
extern "C" {
#endif
    void initialize_logging(int argc, char * argv[]);

    void logging_warning(char *, char*, int);
    void logging_info(char *, char*, int);
    void logging_error(char *, char*, int);
    void logging_warning_if(int , char *, char*, int);
    void logging_info_if(int , char *, char*, int);
    void logging_error_if(int , char *, char*, int);

    void cease_logging();
#ifdef __cplusplus
}
#endif
