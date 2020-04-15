#ifndef IPR_PORT_H
#define IPR_PORT_H

#include <zmq.hpp>
#include <boost/process.hpp>
#include <pxr/usd/sdf/path.h>

#include <thread>

namespace bp = boost::process;

PXR_NAMESPACE_OPEN_SCOPE

class IPRPort {
public:
    class CommandListener {
    public:
        /// This callback should be very lightweight
        /// and eventually can be called from the background thread
        virtual std::string ProcessCommand(std::string const& command) = 0;
    };

    IPRPort(CommandListener* commandListener);
    ~IPRPort();

    void NotifyLayerRemove(std::string const& layerPath);
    void NotifyLayerEdit(std::string const& layerPath, uint64_t timestamp);
    void SendLayer(std::string const& layerPath, uint64_t timestamp, std::string layer);

    void Update();

private:
    void ProcessRequest();

    void SendEnqueuedLayerEdit();
    void SendEnqueuedLayer();

    bool SendLayerEditImpl(std::string const& layerPath, uint64_t timestamp);
    bool SendLayerImpl(std::string const& layerPath, uint64_t timestamp, std::string const& layer);

    bool ConnectionIsOk();

private:
    CommandListener* m_commandListener;

    bp::child m_viewerProcess;

    zmq::socket_t m_controlSocket;
    zmq::socket_t m_notifySocket;

    struct LayerDesc {
        uint64_t timestamp;
        std::string encodedString;

        LayerDesc(uint64_t timestamp, std::string encodedString)
            : timestamp(timestamp), encodedString(std::move(encodedString)) {}
    };
    std::map<std::string, LayerDesc> m_enqueuedLayers;
    std::map<std::string, uint64_t> m_enqueuedLayerEdits;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // IPR_PORT_H
