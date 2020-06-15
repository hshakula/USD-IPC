#ifndef RPR_IMAGING_IPC_API_H
#define RPR_IMAGING_IPC_API_H

#include "pxr/base/arch/export.h"

#if defined(PXR_STATIC)
#   define RPR_IPC_API
#   define RPR_IPC_TEMPLATE_CLASS(...)
#   define RPR_IPC_TEMPLATE_STRUCT(...)
#else
#   if defined(IPC_EXPORTS)
#       define RPR_IPC_API ARCH_EXPORT
#       define RPR_IPC_TEMPLATE_CLASS(...) ARCH_EXPORT_TEMPLATE(class, __VA_ARGS__)
#       define RPR_IPC_TEMPLATE_STRUCT(...) ARCH_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#   else
#       define RPR_IPC_API ARCH_IMPORT
#       define RPR_IPC_TEMPLATE_CLASS(...) ARCH_IMPORT_TEMPLATE(class, __VA_ARGS__)
#       define RPR_IPC_TEMPLATE_STRUCT(...) ARCH_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#   endif
#endif

#endif // RPR_IMAGING_IPC_API_H
