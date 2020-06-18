/*
* Copyright (c) 2019-2020 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "DeviceLoggingContext.h"
#include "ChunkConsumer.h"
#include "Host.h"
#include "DeviceManager.h"
#include "Device.h"

using namespace std;

namespace log_collector
{
    DeviceLoggingContext::DeviceLoggingContext(const std::string& deviceName)
        : m_deviceName(deviceName)
    {
        shared_ptr<Device> device;
        const OperationStatus os = Host::GetHost().GetDeviceManager().GetDeviceByName(m_deviceName, device);
        if (!os)
        {
            LOG_ERROR << "Cannot create device logging context: " << os.GetStatusMessage() << endl;
            return;
        }

        const auto fwStateProvider = device->GetFwStateProvider();
        m_lastfwIdentifier = fwStateProvider->GetFwIdentifier();
        m_fwLogCollector = make_shared<log_collector::LogReader>(m_deviceName, log_collector::CPU_TYPE_FW);
        m_uCodeLogCollector = make_shared<log_collector::LogReader>(m_deviceName, log_collector::CPU_TYPE_UCODE);
    }


    DeviceLoggingContext::~DeviceLoggingContext() = default;

    void DeviceLoggingContext::RestoreLoggingState()
    {
        const auto os = OperationStatus::Merge(
            m_fwLogCollector->RestoreLogging(),
            m_uCodeLogCollector->RestoreLogging());

        if (!os)
        {
            LOG_ERROR << "Cannot restore logging: " << os.GetStatusMessage() << endl;
            return;
        }

        VerifyFwVersion();
        // if Fw is not ready yet, its state will be updated in poll
    }

    bool DeviceLoggingContext::PrepareForLogging()
    {
        bool resFw = m_fwLogCollector->UpdateDeviceInfo();

        bool resuCode = m_uCodeLogCollector->UpdateDeviceInfo();

        return resFw && resuCode;
    }

    void DeviceLoggingContext::UnregisterPollerAndReportDeviceRemoved()
    {
        m_fwLogCollector->UnRegisterPoller();
        m_uCodeLogCollector->UnRegisterPoller();
        m_fwLogCollector->ReportDeviceRemoved();
        m_uCodeLogCollector->ReportDeviceRemoved();
    }

    OperationStatus DeviceLoggingContext::SplitRecordings()
    {
        auto osFw = m_fwLogCollector->SplitRecordings();
        auto osuCode = m_uCodeLogCollector->SplitRecordings();
        return OperationStatus::Merge(osFw, osuCode);
    }


    bool DeviceLoggingContext::VerifyFwVersion()
    {
        shared_ptr<Device> device;
        const OperationStatus os = Host::GetHost().GetDeviceManager().GetDeviceByName(m_deviceName, device);
        if (!os)
        {
            LOG_ERROR << "Cannot verify FW version: " << os.GetStatusMessage() << endl;
            return false; // we want to prepare recording so the validity wil be updated.
        }
        const FwStateProvider* fwStateProvider = device->GetFwStateProvider();
        if (!fwStateProvider->IsInitialized())
        {
            LOG_DEBUG << " FW not initialized, FW state is: " << fwStateProvider->GetFwHealthState() << endl;
            return false;
        }

        const FwIdentifier fwIdentifier = fwStateProvider->GetFwIdentifier();
        if (fwIdentifier != m_lastfwIdentifier)
        {
            LOG_DEBUG << "FW changed on " << m_deviceName << " from " << m_lastfwIdentifier << " to " << fwIdentifier << endl;
        }

        return true;
    }
}
