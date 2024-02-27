request_handlers["getTime"] = [sc](json data) { return sc->getSimulationTime(std::move(data)); };
request_handlers["advanceTime"] = [sc](json data) { return sc->advanceTime(std::move(data)); };
request_handlers["createTask"] = [sc](json data) { return sc->createTask(std::move(data)); };
request_handlers["waitForNextSimulationEvent"] = [sc](json data) { return sc->waitForNextSimulationEvent(std::move(data)); };
request_handlers["simulationEvents"] = [sc](json data) { return sc->getSimulationEvents(std::move(data)); };
request_handlers["hostnames"] = [sc](json data) { return sc->getAllHostnames(std::move(data)); };
request_handlers["inputFiles"] = [sc](json data) { return sc->getTaskInputFiles(std::move(data)); };
request_handlers["addInputFile"] = [sc](json data) { return sc->addInputFile(std::move(data)); };
request_handlers["outputFiles"] = [sc](json data) { return sc->getTaskOutputFiles(std::move(data)); };
request_handlers["addOutputFile"] = [sc](json data) { return sc->addOutputFile(std::move(data)); };
request_handlers["inputFiles"] = [sc](json data) { return sc->getInputFiles(std::move(data)); };
request_handlers["addFile"] = [sc](json data) { return sc->addFile(std::move(data)); };
request_handlers["size"] = [sc](json data) { return sc->getFileSize(std::move(data)); };
request_handlers["tasks"] = [sc](json data) { return sc->getStandardJobTasks(std::move(data)); };
request_handlers["taskGetFlops"] = [sc](json data) { return sc->getTaskFlops(std::move(data)); };
request_handlers["taskGetMinNumCores"] = [sc](json data) { return sc->getTaskMinNumCores(std::move(data)); };
request_handlers["taskGetMaxNumCores"] = [sc](json data) { return sc->getTaskMaxNumCores(std::move(data)); };
request_handlers["taskGetMemory"] = [sc](json data) { return sc->getTaskMemory(std::move(data)); };
request_handlers["taskGetStartDate"] = [sc](json data) { return sc->getTaskStartDate(std::move(data)); };
request_handlers["taskGetEndDate"] = [sc](json data) { return sc->getTaskEndDate(std::move(data)); };
request_handlers["createStandardJob"] = [sc](json data) { return sc->createStandardJob(std::move(data)); };
request_handlers["submit"] = [sc](json data) { return sc->submitStandardJob(std::move(data)); };
request_handlers["addBareMetalComputeService"] = [sc](json data) { return sc->addBareMetalComputeService(std::move(data)); };
request_handlers["addCloudComputeService"] = [sc](json data) { return sc->addCloudComputeService(std::move(data)); };
request_handlers["addBatchComputeService"] = [sc](json data) { return sc->addBatchComputeService(std::move(data)); };
request_handlers["supportsCompoundJobs"] = [sc](json data) { return sc->supportsCompoundJobs(std::move(data)); };
request_handlers["supportsPilotJobs"] = [sc](json data) { return sc->supportsPilotJobs(std::move(data)); };
request_handlers["supportsStandardJobs"] = [sc](json data) { return sc->supportsStandardJobs(std::move(data)); };
request_handlers["addSimpleStorageService"] = [sc](json data) { return sc->addSimpleStorageService(std::move(data)); };
request_handlers["addFileRegistryService"] = [sc](json data) { return sc->addFileRegistryService(std::move(data)); };
request_handlers["addEntry"] = [sc](json data) { return sc->fileRegistryServiceAddEntry(std::move(data)); };
request_handlers["createFileCopy"] = [sc](json data) { return sc->createFileCopyAtStorageService(std::move(data)); };
request_handlers["lookupFile"] = [sc](json data) { return sc->lookupFileAtStorageService(std::move(data)); };
request_handlers["createVM"] = [sc](json data) { return sc->createVM(std::move(data)); };
request_handlers["startVM"] = [sc](json data) { return sc->startVM(std::move(data)); };
request_handlers["shutdownVM"] = [sc](json data) { return sc->shutdownVM(std::move(data)); };
request_handlers["destroyVM"] = [sc](json data) { return sc->destroyVM(std::move(data)); };
request_handlers["isVMRunning"] = [sc](json data) { return sc->isVMRunning(std::move(data)); };
request_handlers["isVMDown"] = [sc](json data) { return sc->isVMDown(std::move(data)); };
request_handlers["suspendVM"] = [sc](json data) { return sc->suspendVM(std::move(data)); };
request_handlers["resumeVM"] = [sc](json data) { return sc->resumeVM(std::move(data)); };
request_handlers["isVMSuspended"] = [sc](json data) { return sc->isVMSuspended(std::move(data)); };
request_handlers["getVMPhysicalHostname"] = [sc](json data) { return sc->getVMPhysicalHostname(std::move(data)); };
request_handlers["getVMComputeService"] = [sc](json data) { return sc->getVMComputeService(std::move(data)); };
request_handlers["getExecutionHosts"] = [sc](json data) { return sc->getExecutionHosts(std::move(data)); };
request_handlers["createWorkflowFromJSON"] = [sc](json data) { return sc->createWorkflowFromJSON(std::move(data)); };
request_handlers["createWorkflow"] = [sc](json data) { return sc->createWorkflow(std::move(data)); };
