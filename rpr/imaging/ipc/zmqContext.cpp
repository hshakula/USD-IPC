#include "zmqContext.h"

#include <pxr/base/tf/instantiateSingleton.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_INSTANTIATE_SINGLETON(zmq::context_t);

zmq::context_t& GetZmqContext() {
    return TfSingleton<zmq::context_t>::GetInstance();
}

PXR_NAMESPACE_CLOSE_SCOPE
