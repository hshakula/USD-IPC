#include "client.h"
#include "boostIncludePath.h"

#include <pxr/base/tf/makePyConstructor.h>
#include <pxr/base/tf/pyPtrHelpers.h>
#include <pxr/base/tf/pyFunction.h>

#include BOOST_INCLUDE_PATH(python.hpp)
#include BOOST_INCLUDE_PATH(python/class.hpp)
#include BOOST_INCLUDE_PATH(python/def.hpp)
#include BOOST_INCLUDE_PATH(python/scope.hpp)
#include BOOST_INCLUDE_PATH(python/call.hpp)

using namespace BOOST_NS::python;

PXR_NAMESPACE_USING_DIRECTIVE

void
wrapClient() {
    typedef void OnStageUpdateCallbackSig();
    using OnStageUpdateCallback = std::function<OnStageUpdateCallbackSig>;
    TfPyFunctionFromPython<OnStageUpdateCallbackSig>();

    using This = RprIpcClient;

    scope s = class_<This, RprIpcClientPtr, BOOST_NS::noncopyable>("Client", no_init)
        .def(TfPyRefAndWeakPtr())
        .def("Create", +[](std::string const& serverAddress, OnStageUpdateCallback onStageUpdateCallback) {
                return RprIpcClient::Create(serverAddress, onStageUpdateCallback);
            },
            return_value_policy<TfPyRefPtrFactory<>>())
        .staticmethod("Create")
        .def("SendCommand", &This::SendCommand)
        .def("GetStage", &This::GetStage)
    ;
}
