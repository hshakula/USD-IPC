#ifndef IPR_PORT_H
#define IPR_PORT_H

#include <pxr/pxr.h>
#include <zmq.hpp>
#include <boost/process.hpp>
#include <thread>

namespace bp = boost::process;

PXR_NAMESPACE_OPEN_SCOPE

class IPRPort {
public:
    class DataSource {
    public:
        virtual ~DataSource() = default;

        virtual std::string GetEncodedStage() = 0;
    };

    IPRPort(DataSource* dataSource);
    ~IPRPort();

    void Update();

    void StageChanged();

private:
    IPRPort();

    void ProcessRequest();
    bool SendStage();

private:
    DataSource* m_dataSource;

    bp::child m_viewerProcess;

    zmq::socket_t m_controlSocket;
    zmq::socket_t m_notifySocket;

    bool m_stageDirty = true;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // IPR_PORT_H
