/**
 * Copyright (c) 2017-2021. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <gtest/gtest.h>
#include <wrench-dev.h>
#include <numeric>

#include "../include/TestWithFork.h"
#include "../include/UniqueTmpPathPrefix.h"

class MultipleWMSTest : public ::testing::Test {

public:
    std::shared_ptr<wrench::CloudComputeService> compute_service = nullptr;
    std::shared_ptr<wrench::StorageService> storage_service = nullptr;

    void do_deferredWMSStartOneWMS_test();

    void do_deferredWMSStartTwoWMS_test();

protected:
    MultipleWMSTest() {
        // Create a platform file
        std::string xml = "<?xml version='1.0'?>"
                          "<!DOCTYPE platform SYSTEM \"https://simgrid.org/simgrid.dtd\">"
                          "<platform version=\"4.1\"> "
                          "   <zone id=\"AS0\" routing=\"Full\"> "
                          "       <host id=\"DualCoreHost\" speed=\"1f\" core=\"2\"> "
                          "          <disk id=\"large_disk\" read_bw=\"100MBps\" write_bw=\"100MBps\">"
                          "             <prop id=\"size\" value=\"100B\"/>"
                          "             <prop id=\"mount\" value=\"/\"/>"
                          "          </disk>"
                          "          <disk id=\"scratch\" read_bw=\"100MBps\" write_bw=\"100MBps\">"
                          "             <prop id=\"size\" value=\"100B\"/>"
                          "             <prop id=\"mount\" value=\"/scratch\"/>"
                          "          </disk>"
                          "       </host>"
                          "       <host id=\"QuadCoreHost\" speed=\"1f\" core=\"4\"> "
                          "          <disk id=\"large_disk\" read_bw=\"100MBps\" write_bw=\"100MBps\">"
                          "             <prop id=\"size\" value=\"100B\"/>"
                          "             <prop id=\"mount\" value=\"/\"/>"
                          "          </disk>"
                          "          <disk id=\"scratch\" read_bw=\"100MBps\" write_bw=\"100MBps\">"
                          "             <prop id=\"size\" value=\"100B\"/>"
                          "             <prop id=\"mount\" value=\"/scratch\"/>"
                          "          </disk>"
                          "       </host>"
                          "       <link id=\"1\" bandwidth=\"5000GBps\" latency=\"0us\"/>"
                          "       <route src=\"DualCoreHost\" dst=\"QuadCoreHost\"> <link_ctn id=\"1\"/> </route>"
                          "   </zone> "
                          "</platform>";
        FILE *platform_file = fopen(platform_file_path.c_str(), "w");
        fprintf(platform_file, "%s", xml.c_str());
        fclose(platform_file);
    }

    std::shared_ptr<wrench::Workflow> createWorkflow(std::string prefix) {
        std::shared_ptr<wrench::Workflow> workflow;
        std::shared_ptr<wrench::DataFile> input_file;
        std::shared_ptr<wrench::DataFile> output_file1;
        std::shared_ptr<wrench::DataFile> output_file2;
        std::shared_ptr<wrench::WorkflowTask> task1;
        std::shared_ptr<wrench::WorkflowTask> task2;

        // Create the simplest workflow
        workflow = wrench::Workflow::createWorkflow();
        //        workflows.push_back(workflow);

        // Create the files
        input_file = wrench::Simulation::addFile(prefix + "_input_file", 10);
        output_file1 = wrench::Simulation::addFile(prefix + "output_file1", 10);
        output_file2 = wrench::Simulation::addFile(prefix + "output_file2", 10);

        // Create the tasks
        task1 = workflow->addTask("task_1_10s_1core", 10.0, 1, 1, 0);
        task2 = workflow->addTask("task_2_10s_1core", 10.0, 1, 1, 0);

        // Add file-task1 dependencies
        task1->addInputFile(input_file);
        task2->addInputFile(input_file);

        task1->addOutputFile(output_file1);
        task2->addOutputFile(output_file2);

        return workflow;
    }
    //    std::vector<std::shared_ptr<wrench::Workflow>> workflows;
    std::string platform_file_path = UNIQUE_TMP_PATH_PREFIX + "platform.xml";
};

/**********************************************************************/
/**  DEFERRED WMS START TIME WITH ONE WMS ON ONE HOST                **/
/**********************************************************************/

class DeferredWMSStartTestWMS : public wrench::ExecutionController {

public:
    DeferredWMSStartTestWMS(MultipleWMSTest *test,
                            std::shared_ptr<wrench::Workflow> workflow,
                            double sleep_time,
                            std::string &hostname) : wrench::ExecutionController(hostname, "test"), test(test), workflow(workflow), sleep_time(sleep_time) {
    }

private:
    MultipleWMSTest *test;
    std::shared_ptr<wrench::Workflow> workflow;
    double sleep_time;


    int main() override {
        // check for deferred start
        wrench::Simulation::sleep(this->sleep_time);


        // Create a data movement manager
        auto data_movement_manager = this->createDataMovementManager();

        // Create a job manager
        auto job_manager = this->createJobManager();


        // Get the cloud service
        auto cs = this->test->compute_service;

        std::vector<std::tuple<std::shared_ptr<wrench::FileLocation>, std::shared_ptr<wrench::FileLocation>>> pre_copies = {};
        for (auto it: this->workflow->getInputFileMap()) {
            std::tuple<std::shared_ptr<wrench::FileLocation>, std::shared_ptr<wrench::FileLocation>> each_copy =
                    std::make_tuple(
                            wrench::FileLocation::LOCATION(this->test->storage_service, it.second),
                            wrench::FileLocation::SCRATCH(it.second));
            pre_copies.push_back(each_copy);
        }

        // Create a 2-task1 job
        auto two_task_job = job_manager->createStandardJob(this->workflow->getTasks(),
                                                           (std::map<std::shared_ptr<wrench::DataFile>, std::shared_ptr<wrench::FileLocation>>){},
                                                           pre_copies,
                                                           {}, {});

        // Submit the 2-task1 job for execution
        try {
            auto cs = this->test->compute_service;
            auto vm_name = cs->createVM(2, 100);
            auto vm_cs = cs->startVM(vm_name);
            job_manager->submitJob(two_task_job, vm_cs);
        } catch (wrench::ExecutionException &e) {
            throw std::runtime_error(e.what());
        }

        // Wait for a workflow execution event
        std::shared_ptr<wrench::ExecutionEvent> event;
        try {
            event = this->waitForNextEvent();
        } catch (wrench::ExecutionException &e) {
            throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        if (not std::dynamic_pointer_cast<wrench::StandardJobCompletedEvent>(event)) {
            throw std::runtime_error("Unexpected workflow execution event: " + event->toString());
        }

        return 0;
    }
};

TEST_F(MultipleWMSTest, DeferredWMSStartTestWMS) {
    DO_TEST_WITH_FORK(do_deferredWMSStartOneWMS_test);
    DO_TEST_WITH_FORK(do_deferredWMSStartTwoWMS_test);
}

void MultipleWMSTest::do_deferredWMSStartOneWMS_test() {
    // Create and initialize a simulation
    auto simulation = wrench::Simulation::createSimulation();
    int argc = 1;
    auto argv = (char **) calloc(argc, sizeof(char *));
    argv[0] = strdup("unit_test");

    ASSERT_NO_THROW(simulation->init(&argc, argv));

    // Setting up the platform
    ASSERT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

    // Get a hostname
    std::string hostname = wrench::Simulation::getHostnameList()[0];

    // Create a Storage Service
    ASSERT_NO_THROW(storage_service = simulation->add(
                            wrench::SimpleStorageService::createSimpleStorageService(hostname, {"/"})));

    // Create a Cloud Service
    std::vector<std::string> execution_hosts = {wrench::Simulation::getHostnameList()[1]};
    ASSERT_NO_THROW(compute_service = simulation->add(new wrench::CloudComputeService(
                            hostname, execution_hosts, "/scratch",
                            {})));

    // Create a WMS
    auto workflow = this->createWorkflow("wf_");
    std::shared_ptr<wrench::ExecutionController> wms = nullptr;

    ASSERT_NO_THROW(wms = simulation->add(
                            new DeferredWMSStartTestWMS(this, workflow, 100, hostname)));

    // Create a file registry
    ASSERT_NO_THROW(simulation->add(
            new wrench::FileRegistryService(hostname)));

    // Staging the input_file on the storage service
    for (auto const &f: workflow->getInputFiles()) {
        ASSERT_NO_THROW(storage_service->createFile(f));
    }

    // Running a "run a single task1" simulation
    ASSERT_NO_THROW(simulation->launch());

    // Simulation trace
    ASSERT_GT(wrench::Simulation::getCurrentSimulatedDate(), 100);


    for (int i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
}

void MultipleWMSTest::do_deferredWMSStartTwoWMS_test() {
    // Create and initialize a simulation
    auto simulation = wrench::Simulation::createSimulation();
    int argc = 1;
    auto argv = (char **) calloc(argc, sizeof(char *));
    argv[0] = strdup("unit_test");

    ASSERT_NO_THROW(simulation->init(&argc, argv));

    // Setting up the platform
    ASSERT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

    // Get a hostname
    std::string hostname = wrench::Simulation::getHostnameList()[0];

    // Create a Storage Service
    ASSERT_NO_THROW(storage_service = simulation->add(
                            wrench::SimpleStorageService::createSimpleStorageService(hostname, {"/"})));

    // Create a Cloud Service
    std::vector<std::string> execution_hosts = {wrench::Simulation::getHostnameList()[1]};
    ASSERT_NO_THROW(compute_service = simulation->add(
                            new wrench::CloudComputeService(hostname, execution_hosts, "/scratch",
                                                            {})));

    // Create a WMS
    auto workflow = this->createWorkflow("wf1_");
    std::shared_ptr<wrench::ExecutionController> wms1 = nullptr;
    ASSERT_NO_THROW(wms1 = simulation->add(
                            new DeferredWMSStartTestWMS(this, workflow, 100, hostname)));

    // Create a second WMS
    auto workflow2 = this->createWorkflow("wf_2");
    std::shared_ptr<wrench::ExecutionController> wms2 = nullptr;
    ASSERT_NO_THROW(wms2 = simulation->add(
                            new DeferredWMSStartTestWMS(this, workflow2, 10000, hostname)));

    // Create a file registry
    ASSERT_NO_THROW(simulation->add(
            new wrench::FileRegistryService(hostname)));

    // Staging the input_file on the storage service
    for (auto const &f: workflow->getInputFiles()) {
        ASSERT_NO_THROW(storage_service->createFile(f));
    }
    for (auto const &f: workflow2->getInputFiles()) {
        ASSERT_NO_THROW(storage_service->createFile(f));
    }

    // Running a "run a single task1" simulation
    ASSERT_NO_THROW(simulation->launch());

    // Simulation trace
    ASSERT_GT(wrench::Simulation::getCurrentSimulatedDate(), 1000);


    for (int i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
}
