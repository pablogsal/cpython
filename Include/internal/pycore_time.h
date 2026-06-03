#ifndef Py_INTERNAL_TIME_H
#define Py_INTERNAL_TIME_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif


struct _time_runtime_state {
#ifdef HAVE_TIMES
    int ticks_per_second_initialized;
    long ticks_per_second;
#else
    int _not_used;
#endif
};

extern int _PyTime_GetProcessTimeWithInfo(
    _PyTime_t *t,
    _Py_clock_info_t *info);


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_TIME_H */
