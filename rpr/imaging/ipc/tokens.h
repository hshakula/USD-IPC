#ifndef RPR_IMAGING_IPC_TOKENS_H
#define RPR_IMAGING_IPC_TOKENS_H

#include <pxr/pxr.h>
#include <rpr/imaging/ipc/api.h>
#include <pxr/base/tf/staticTokens.h>

PXR_NAMESPACE_OPEN_SCOPE

#define RPR_IPC_TOKENS \
    (connect) \
    (disconnect) \
    (shutdown) \
    (ping) \
    (layer) \
    (layerRemove) \
    (ok) \
    (fail)

TF_DECLARE_PUBLIC_TOKENS(RprIpcTokens, RPR_IPC_API, RPR_IPC_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // RPR_IMAGING_IPC_TOKENS_H
