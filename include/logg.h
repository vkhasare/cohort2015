#ifdef __cplusplus
extern "C" {
#endif
    void initialize_logging(int argc, char * argv[]);

    void LOGGING_WARNING(char *);
    void LOGGING_INFO(char *);
    void LOGGING_ERROR(char *);
    void LOGGING_WARNING_IF(int , char *);
    void LOGGING_INFO_IF(int , char *);
    void LOGGING_ERROR_IF(int , char *);

    void cease_logging();
#ifdef __cplusplus
}
#endif
