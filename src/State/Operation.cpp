#include "State/Operation.hpp"

// Project includes
#include "DevelopmentStuff/DataSimulation.h"
#include "Sensor/FuelLevel.hpp"
#include "Sensor/LeftIndicator.hpp"
#include "Sensor/OilPressure.hpp"
#include "Sensor/RightIndicator.hpp"
#include "Sensor/Rpm.hpp"
#include "Sensor/Speed.hpp"
#include "Sensor/WaterTemperature.hpp"
#include "Wifi.hpp"
#include "WifiHost.hpp"
#include "WifiJoin.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "Operation";

constexpr auto PASSIVE_SENSOR_POLL_HZ = 0.1;
constexpr auto BROADCAST_SENSOR_DATA_HZ = 100;

/*
 *	Private Static Task
 */
void staticReadPassiveSensorsTask(void* param)
{
    if (param == nullptr)
    {
        return;
    }

    Operation* instance = static_cast<Operation*>(param);

    instance->readPassiveSensorsTask();
}

void staticBroadcastSensorDataTask(void* param)
{
    if (param == nullptr)
    {
        return;
    }

    Operation* instance = static_cast<Operation*>(param);

    instance->broadcastSensorsTask();
}

/*
 *	Public Function implementations
 */
Operation::Operation() :
    State(State::OPERATION), readPassiveSensorsTaskHandle_(nullptr), broadCastSensorDataTaskHandle_(nullptr)
{
    config_ = core_->getConfig();

    // Install ISR Service for the Active Sensors
    if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install the ISR service");
    }

    /*
     *	Simulate Data
     */
    if ((*config_)["Simulation"])
    {
        simulation_ = (*config_)["Simulation"].as<bool>();
    }

    if (simulation_)
    {
        simulationData_ = generateSimulationData();
    }
}

Operation::~Operation()
{
    vTaskDelete(readPassiveSensorsTaskHandle_);
    vTaskDelete(broadCastSensorDataTaskHandle_);

    for (auto& sensor : passiveSensor_)
    {
        free(sensor);
    }

    for (auto& sensor : activeSensor_)
    {
        sensor->disable();
        free(sensor);
    }
}

void Operation::enter()
{
    /*
     *	Setup passive sensors
     */
    const auto adc1Handle = core_->getAdc();
    passiveSensor_ = {
        new FuelLevel(adc1Handle),
        new OilPressure(adc1Handle),
        new WaterTemperature(adc1Handle),
    };

    /*
     *	Setup active sensors
     */
    activeSensor_ = {
        new Rpm(),
        new Speed(),
        new LeftIndicator(),
        new RightIndicator(),
    };
    for (const auto& sensor : activeSensor_)
    {
        sensor->enable();
    }

    /*
     *	Setup read & broadcast task
     */
    if (xTaskCreate(staticReadPassiveSensorsTask, "OperationReadPassiveSensorsTask", 2048, this, 2,
                    &readPassiveSensorsTaskHandle_) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task for reading all passive HW sensors");
    }

    if (xTaskCreate(staticBroadcastSensorDataTask, "OperationBroadcastSensorDataTask", 2048 * 2, this, 2,
                    &broadCastSensorDataTaskHandle_) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task for broadcasting sensor data");
    }

    /*
     *	Wifi
     */
    Wifi::WIFI_TYPE wifiType = Wifi::WIFI_TYPE::HOST;
    if ((*config_)["ForceWifiMode"])
    {
        wifiType = static_cast<Wifi::WIFI_TYPE>((*config_)["ForceWifiMode"].as<int>());
    }

    Wifi* wifi = nullptr;

    // Host
    if (wifiType == Wifi::WIFI_TYPE::HOST)
    {
        // Create Wifi
        wifi = new WifiHost();
        core_->setWifi(wifi);

        // Initialize Wifi
        wifi->setSSID((*config_)["WifiHost"]["ssid"]);
        wifi->setPassword((*config_)["WifiHost"]["password"]);
        wifi->callOnSuccess([this]
        {
            WebInterface* webInterface = new WebInterface();
            core_->setWebinterface(webInterface);

            this->setupDisplayWifi();
        });

        wifi->start();
    }

    // Join
    else if (wifiType == Wifi::WIFI_TYPE::JOIN)
    {
        // Create Wifi
        wifi = new WifiJoin();
        core_->setWifi(wifi);

        // Initialize Wifi
        wifi->setSSID((*config_)["WifiJoin"]["ssid"]);
        wifi->setPassword((*config_)["WifiJoin"]["password"]);
        wifi->callOnSuccess([this]
        {
            WebInterface* webInterface = new WebInterface();
            core_->setWebinterface(webInterface);

            this->setupDisplayWifi();
        });

        wifi->start();
    }

    // Error
    else
    {
        ESP_LOGW(TAG, "Failed to initialize wifi. Continuing without it");
        return;
    }
}

void Operation::handleCanFrame(const Can::Frame& frame)
{
    if (blocked)
    {
        return;
    }

    if (frame.group != CanFrame::GROUP::WIFI)
    {
        return;
    }

    // Act depending on the function type
    if (frame.group == CanFrame::WIFI)
    {
        // Act depending on the function type
        switch (frame.function)
        {
        case CanFrame::WIFI::JOIN_WIFI:
            {
                if (!frame.answer)
                {
                    return;
                }

                static uint8_t counter = 0;
                esp_rom_printf("Display %d joined Wifi\n", ++counter);
            }
            break;

        case CanFrame::WIFI::EXECUTE_UPDATE:
            {
                if (!frame.answer)
                {
                    return;
                }

                static uint8_t counter = 0;
                ESP_LOGI(TAG, "Display %d executed update successfully!", frame.sender);

                // Restart all displays & ourselves when they are ready
                if (++counter >= 3)
                {
                    Can::Frame txFrame;
                    txFrame.sender = CAN_MASTER_ID;
                    txFrame.target = CAN_BROADCAST_ID;
                    txFrame.group = CanFrame::GROUP::CONFIGURATION;
                    txFrame.function = CanFrame::CONFIGURATION::RESTART;

                    Core::get()->getCan()->queueFrame(txFrame);

                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
                else
                {
                    executeDisplayUpdate(core_->getDisplays()->at(counter).getCanId());
                }
            }
            break;

        default:
            break;
        }
    }
}

void Operation::readPassiveSensorsTask() const
{
    while (true)
    {
        for (auto& sensor : passiveSensor_)
        {
            sensor->read();
        }

        vTaskDelay(pdMS_TO_TICKS(1000.0 / PASSIVE_SENSOR_POLL_HZ));
    }
}

void Operation::broadcastSensorsTask() const
{
    Can::Frame frame;
    frame.sender = CAN_MASTER_ID;
    frame.target = CAN_BROADCAST_ID;
    frame.group = CanFrame::GROUP::SENSOR;
    frame.function = CanFrame::SENSOR::BROADCAST_DATA;
    frame.dataLengthCode = 8;
    frame.answer = false;

    uint8_t lastData[8] = {0x00};

    while (true)
    {
        if (!simulation_)
        {
            // Fuel Level, Oil Pressure, Water Temperature
            for (uint8_t i = 0; i < passiveSensor_.size(); i++)
            {
                frame.data[i] = passiveSensor_.at(i)->get();
            }
            // frame.data[0] = static_cast<uint8_t>(esp_random() % 101);

            // RPM
            const auto& rpm = activeSensor_.at(0)->get();
            frame.data[3] = rpm >> 8;
            frame.data[4] = rpm & 0xFF;

            // Speed
            frame.data[5] = activeSensor_.at(1)->get();

            // Left Indicator
            frame.data[6] = activeSensor_.at(2)->get();

            // Right Indicator
            frame.data[7] = activeSensor_.at(3)->get();

            // Did the data stay the same?
            bool equal = true;
            for (uint8_t i = 0; i < frame.dataLengthCode; i++)
            {
                equal &= frame.data[i] == lastData[i];
            }

            if (!equal)
            {
                core_->getCan()->queueFrame(frame);
                memcpy(lastData, frame.data, frame.dataLengthCode);
            }

            vTaskDelay(pdMS_TO_TICKS(1000 / BROADCAST_SENSOR_DATA_HZ));
        } else
        {
            static unsigned int frameIndex = 0;

            lastData[0] = simulationData_[frameIndex][0];
            lastData[1] = simulationData_[frameIndex][1];
            lastData[2] = simulationData_[frameIndex][2];
            lastData[3] = simulationData_[frameIndex][3];
            lastData[4] = simulationData_[frameIndex][4];
            lastData[5] = simulationData_[frameIndex][5];
            lastData[6] = simulationData_[frameIndex][6];
            lastData[7] = simulationData_[frameIndex][7];
            core_->getCan()->queueFrame(frame);

            frameIndex++;
            vTaskDelay(pdMS_TO_TICKS(1000 / 60));
        }
    }
}

/*
 *	Private Function Implementations
 */
void Operation::setupDisplayWifi() const
{
    ESP_LOGI(TAG, "Starting to transmit SSID and Password to the displays");

    /*
     *	Transmit own IP
     */
    Can::Frame transmitMasterIpFrame;
    transmitMasterIpFrame.sender = CAN_MASTER_ID;
    transmitMasterIpFrame.target = CAN_BROADCAST_ID;
    transmitMasterIpFrame.group = CanFrame::GROUP::WIFI;
    transmitMasterIpFrame.function = CanFrame::WIFI::SET_MASTER_IP;
    transmitMasterIpFrame.dataLengthCode = 4;

    const auto& ip = core_->getWifi()->getIp();
    transmitMasterIpFrame.data[0] = ip[0];
    transmitMasterIpFrame.data[1] = ip[1];
    transmitMasterIpFrame.data[2] = ip[2];
    transmitMasterIpFrame.data[3] = ip[3];
    Core::get()->getCan()->queueFrame(transmitMasterIpFrame);

    /*
     *	SSID
     */
    // Split the ssid up into packages with a size of max 8 bytes/chars
    const auto& ssid = core_->getWifi()->getSSID();
    std::vector<std::vector<char>> allSsidPackages;
    std::vector<char> ssidPackage;
    for (const auto& c : ssid)
    {
        if (ssidPackage.size() >= 8)
        {
            allSsidPackages.push_back(ssidPackage);
            ssidPackage.clear();
        }

        ssidPackage.push_back(c);
    }
    allSsidPackages.push_back(ssidPackage); // Add the last package too

    // Transmit all packages to the displays
    for (const auto& package : allSsidPackages)
    {
        Can::Frame ssidPackageFrame;
        ssidPackageFrame.sender = CAN_MASTER_ID;
        ssidPackageFrame.target = CAN_BROADCAST_ID;
        ssidPackageFrame.group = CanFrame::GROUP::WIFI;
        ssidPackageFrame.function = CanFrame::WIFI::SET_SSID;
        ssidPackageFrame.dataLengthCode = package.size();
        std::copy(package.begin(), package.end(), ssidPackageFrame.data);

        Core::get()->getCan()->queueFrame(ssidPackageFrame);
    }

    /*
     *	Password
     */
    // Split the password up into packages with a size of max 8 bytes/chars
    const auto& password = core_->getWifi()->getPassword();
    std::vector<std::vector<char>> allPsswdPackages;
    std::vector<char> psswdPackage;
    for (const auto& c : password)
    {
        if (psswdPackage.size() >= 8)
        {
            allPsswdPackages.push_back(psswdPackage);
            psswdPackage.clear();
        }

        psswdPackage.push_back(c);
    }
    allPsswdPackages.push_back(psswdPackage); // Add the last package too

    // Transmit all packages to the displays
    for (const auto& package : allPsswdPackages)
    {
        Can::Frame passwordPackageFrame;
        passwordPackageFrame.sender = CAN_MASTER_ID;
        passwordPackageFrame.target = CAN_BROADCAST_ID;
        passwordPackageFrame.group = CanFrame::GROUP::WIFI;
        passwordPackageFrame.function = CanFrame::WIFI::SET_PASSWORD;
        passwordPackageFrame.dataLengthCode = package.size();
        std::copy(package.begin(), package.end(), passwordPackageFrame.data);

        Core::get()->getCan()->queueFrame(passwordPackageFrame);
    }

    /*
     *	Join Wifi
     */
    Can::Frame joinWifiFrame;
    joinWifiFrame.sender = CAN_MASTER_ID;
    joinWifiFrame.target = CAN_BROADCAST_ID;
    joinWifiFrame.group = CanFrame::GROUP::WIFI;
    joinWifiFrame.function = CanFrame::WIFI::JOIN_WIFI;
    joinWifiFrame.dataLengthCode = 0;

    Core::get()->getCan()->queueFrame(joinWifiFrame);
}

void Operation::executeDisplayUpdate(const uint8_t displayId) const
{
    ESP_LOGI(TAG, "Executing update for display with ID %d!", displayId);

    Can::Frame txFrame;
    txFrame.sender = CAN_MASTER_ID;
    txFrame.target = displayId;
    txFrame.group = CanFrame::GROUP::WIFI;
    txFrame.function = CanFrame::WIFI::EXECUTE_UPDATE;

    Core::get()->getCan()->queueFrame(txFrame);
}
