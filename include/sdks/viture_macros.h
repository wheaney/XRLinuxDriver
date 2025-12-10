#ifndef VITURE_MACROS_H
#define VITURE_MACROS_H

// Expose public SDK symbols while allowing projects to override via VITURE_STATIC_DEFINE
#if defined(_WIN32) || defined(__CYGWIN__)
#    ifdef VITURE_STATIC_DEFINE
#        define VITURE_API
#    elif defined(VITURE_GLASSES_BUILD)
#        define VITURE_API __declspec(dllexport)
#    else
#        define VITURE_API __declspec(dllimport)
#    endif
#else
#    if defined(__GNUC__) && __GNUC__ >= 4
#        define VITURE_API __attribute__((visibility("default")))
#    else
#        define VITURE_API
#    endif
#endif

#endif // VITURE_MACROS_H
