#ifndef PLUGIN_H
#define PLUGIN_H

#include "dcc.h"
#include "iprPort.h"

#include <pxr/usd/usd/stage.h>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class Plugin : public IPRPort::DataSource {
public:
    Plugin(DCC& dcc);

    void Update();

    // IPRPort::Data source implementation
    std::string GetEncodedStage() override;

private:
    void UpdateStage();

private:
    DCC& m_dcc;

    UsdStageRefPtr m_stage;

    std::unique_ptr<IPRPort> m_iprPort;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PLUGIN_H
