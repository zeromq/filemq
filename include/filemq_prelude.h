//- Non-portable declaration specifiers -------------------------------------

#define LIBFMQ_EXPORTS // temporary declaration

#if defined (__WINDOWS__)
#   if defined LIBFMQ_STATIC
#       define FMQ_EXPORT
#   elif defined LIBFMQ_EXPORTS
#       define FMQ_EXPORT __declspec(dllexport)
#   else
#       define FMQ_EXPORT __declspec(dllimport)
#   endif
#else
#   define FMQ_EXPORT
#endif


