#include "debugCodes.h"

#include "pxr/base/tf/registryManager.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfDebug) {
    TF_DEBUG_ENVIRONMENT_SYMBOL(HD_USD_IPC_DEBUG_IPR_COMMANDS, "dump incomming IPR commands");
}

PXR_NAMESPACE_CLOSE_SCOPE
