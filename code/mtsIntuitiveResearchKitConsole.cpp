/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
  $Id: mtsIntuitiveResearchKitConsole.cpp 4138 2013-04-27 21:48:29Z adeguet1 $

  Author(s):  Anton Deguet
  Created on: 2013-05-17

  (C) Copyright 2013 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/

// system include
#include <iostream>

// cisst
#include <cisstMultiTask/mtsInterfaceRequired.h>
#include <cisstMultiTask/mtsInterfaceProvided.h>

#include <sawRobotIO1394/mtsRobotIO1394.h>
#include <sawControllers/mtsPID.h>

#include <sawIntuitiveResearchKit/mtsIntuitiveResearchKitMTM.h>
#include <sawIntuitiveResearchKit/mtsIntuitiveResearchKitPSM.h>
#include <sawIntuitiveResearchKit/mtsIntuitiveResearchKitConsole.h>

CMN_IMPLEMENT_SERVICES(mtsIntuitiveResearchKitConsole);


mtsIntuitiveResearchKitConsole::Arm::Arm(const std::string & name, const std::string & ioComponentName):
    Name_(name),
    IOComponentName_(ioComponentName)
{}

void mtsIntuitiveResearchKitConsole::Arm::ConfigurePID(const std::string & configFile,
                                                       const double & periodInSeconds)
{
    PIDComponentName_ = Name_ + "-PID";
    PIDConfigurationFile_ = configFile;

    mtsManagerLocal * componentManager = mtsManagerLocal::GetInstance();
    mtsPID * pidMaster = new mtsPID(PIDComponentName_,
                                    (periodInSeconds != 0.0) ? periodInSeconds : 1.0 * cmn_s);
    pidMaster->Configure(PIDConfigurationFile_);
    componentManager->AddComponent(pidMaster);
    componentManager->Connect(PIDComponentName(), "RobotJointTorqueInterface", IOComponentName(), Name());
    if (periodInSeconds == 0.0) {
        componentManager->Connect(PIDComponentName(), "ExecIn",
                                  IOComponentName(), "ExecOut");
    }
}

void mtsIntuitiveResearchKitConsole::Arm::ConfigureArm(const ArmType armType,
                                                       const std::string & configFile,
                                                       const double & periodInSeconds)
{
    mtsManagerLocal * componentManager = mtsManagerLocal::GetInstance();
    ArmConfigurationFile_ = configFile;
    if (armType == ARM_MTM) {
        mtsIntuitiveResearchKitMTM * master = new mtsIntuitiveResearchKitMTM(Name(), periodInSeconds);
        master->Configure(ArmConfigurationFile_);
        componentManager->AddComponent(master);
    } else {
        mtsIntuitiveResearchKitPSM * slave = new mtsIntuitiveResearchKitPSM(Name(), periodInSeconds);
        slave->Configure(ArmConfigurationFile_);
        componentManager->AddComponent(slave);
        componentManager->Connect(Name(), "Adapter",
                                  IOComponentName(), Name() + "-Adapter");
        componentManager->Connect(Name(), "Tool",
                                  IOComponentName(), Name() + "-Tool");
        componentManager->Connect(Name(), "ManipClutch",
                                  IOComponentName(), Name() + "-ManipClutch");
        componentManager->Connect(Name(), "SUJClutch",
                                  IOComponentName(), Name() + "-SUJClutch");
    }

    componentManager->Connect(Name(), "PID",
                              PIDComponentName(), "Controller");
    componentManager->Connect(Name(), "RobotIO",
                              IOComponentName(), Name());
}

const std::string & mtsIntuitiveResearchKitConsole::Arm::Name(void) const {
    return Name_;
}

const std::string & mtsIntuitiveResearchKitConsole::Arm::IOComponentName(void) const {
    return IOComponentName_;
}

const std::string & mtsIntuitiveResearchKitConsole::Arm::PIDComponentName(void) const {
    return PIDComponentName_;
}

mtsIntuitiveResearchKitConsole::mtsIntuitiveResearchKitConsole(const std::string & componentName):
    mtsTaskFromSignal(componentName, 100)
{
    mtsInterfaceProvided * interfaceProvided = AddInterfaceProvided("Main");
    if (interfaceProvided) {
        interfaceProvided->AddCommandWrite(&mtsIntuitiveResearchKitConsole::SetRobotControlState, this,
                                           "SetRobotControlState", std::string(""));
        interfaceProvided->AddEventWrite(ErrorMessageEventTrigger, "RobotErrorMsg", std::string(""));
        interfaceProvided->AddEventWrite(StatusMessageEventTrigger, "RobotStatusMsg", std::string(""));
    }
}

void mtsIntuitiveResearchKitConsole::Configure(const std::string & filename)
{
    CMN_LOG_CLASS_INIT_VERBOSE << "Configure: " << filename << std::endl;
}

void mtsIntuitiveResearchKitConsole::Startup(void)
{
    CMN_LOG_CLASS_INIT_VERBOSE << "mtsIntuitiveResearchKitConsole::Startup" << std::endl;
}

void mtsIntuitiveResearchKitConsole::Run(void)
{
    ProcessQueuedCommands();
    ProcessQueuedEvents();
}

void mtsIntuitiveResearchKitConsole::Cleanup(void)
{
    CMN_LOG_CLASS_INIT_VERBOSE << "mtsIntuitiveResearchKitConsole::Cleanup" << std::endl;
}

bool mtsIntuitiveResearchKitConsole::AddArm(Arm * newArm)
{
    if (newArm->PIDConfigurationFile_.empty() || newArm->ArmConfigurationFile_.empty()) {
        CMN_LOG_CLASS_INIT_ERROR << "AddArm, " << newArm->Name() << " must be configured first (PID and Arm config)." << std::endl;
        return false;
    }
    // create new required interfaces to communicate with the components we created
    newArm->InterfaceRequired = this->AddInterfaceRequired(newArm->Name());
    if (newArm->InterfaceRequired) {
        newArm->InterfaceRequired->AddFunction("SetRobotControlState", newArm->SetRobotControlState);
        newArm->InterfaceRequired->AddEventHandlerWrite(&mtsIntuitiveResearchKitConsole::ErrorMessageEventHandler, this, "RobotErrorMsg");
        newArm->InterfaceRequired->AddEventHandlerWrite(&mtsIntuitiveResearchKitConsole::StatusMessageEventHandler, this, "RobotStatusMsg");
        mtsManagerLocal * componentManager = mtsManagerLocal::GetInstance();
        componentManager->Connect(this->GetName(), newArm->Name(),
                                  newArm->Name(), "Robot");

        this->Arms.push_back(newArm);
        return true;
    }
    CMN_LOG_CLASS_INIT_ERROR << "AddArm, unable to add new arm.  Are you adding two arms with the same name? "
                             << newArm->Name() << std::endl;
    return false;
}

void mtsIntuitiveResearchKitConsole::SetRobotControlState(const std::string & newState)
{
    mtsExecutionResult result;
    const MTMList::iterator end = Arms.end();
    for (MTMList::iterator arm = Arms.begin();
         arm != end;
         ++arm) {
        result = (*arm)->SetRobotControlState(newState);
        if (!result) {
            CMN_LOG_CLASS_RUN_ERROR << "SetRobotControlState: failed to set state \""
                                    << newState << "\" for arm \"" << (*arm)->Name()
                                    << "\"" << std::endl;
        }
    }
}

void mtsIntuitiveResearchKitConsole::ErrorMessageEventHandler(const std::string & message) {
    ErrorMessageEventTrigger(message);
}

void mtsIntuitiveResearchKitConsole::StatusMessageEventHandler(const std::string & message) {
    StatusMessageEventTrigger(message);
}
