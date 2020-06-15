#ifndef RPR_IMAGING_IPC_ZMQ_CONTEXT_H
#define RPR_IMAGING_IPC_ZMQ_CONTEXT_H

#include <zmq.hpp>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

zmq::context_t& GetZmqContext();

PXR_NAMESPACE_CLOSE_SCOPE

#endif // RPR_IMAGING_IPC_ZMQ_CONTEXT_H
