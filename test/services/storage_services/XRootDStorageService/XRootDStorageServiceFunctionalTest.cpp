/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */


#include <gtest/gtest.h>

#include <wrench-dev.h>
#include "../../../include/TestWithFork.h"
#include "../../../include/UniqueTmpPathPrefix.h"


WRENCH_LOG_CATEGORY(xrootd_storage_service_functional_test, "Log category for XRootDServiceFunctionalTest");


class XRootDServiceFunctionalTest : public ::testing::Test {

public:

    void do_BasicFunctionality_test();

    std::shared_ptr<wrench::XRootD::Node> supervisor;

protected:

    XRootDServiceFunctionalTest() {



        // Create a one-host platform file
        std::string xml = "<?xml version='1.0'?>"
                          "<!DOCTYPE platform SYSTEM \"https://simgrid.org/simgrid.dtd\">"
                          "<platform version=\"4.1\"> "
                          "   <zone id=\"AS0\" routing=\"Full\"> "
                          "       <host id=\"Host1\" speed=\"1f\">"
                          "          <disk id=\"large_disk\" read_bw=\"100MBps\" write_bw=\"100MBps\">"
                          "             <prop id=\"size\" value=\"100B\"/>"
                          "             <prop id=\"mount\" value=\"/disk100/\"/>"
                          "          </disk>"
                          "       </host>"
                          "       <host id=\"Host2\" speed=\"1f\">"
                          "          <disk id=\"large_disk\" read_bw=\"100MBps\" write_bw=\"100MBps\">"
                          "             <prop id=\"size\" value=\"100B\"/>"
                          "             <prop id=\"mount\" value=\"/disk100/\"/>"
                          "          </disk>"
                          "       </host>"
                          "       <host id=\"Host3\" speed=\"1f\">"
                          "          <disk id=\"large_disk\" read_bw=\"100MBps\" write_bw=\"100MBps\">"
                          "             <prop id=\"size\" value=\"100B\"/>"
                          "             <prop id=\"mount\" value=\"/disk100/\"/>"
                          "          </disk>"
                          "       </host>"
                          "       <link id=\"link12\" bandwidth=\"5GBps\" latency=\"10us\"/>"
                          "       <link id=\"link13\" bandwidth=\"5GBps\" latency=\"10us\"/>"
                          "       <link id=\"link23\" bandwidth=\"5GBps\" latency=\"10us\"/>"
                          "       <route src=\"Host1\" dst=\"Host2\"> <link_ctn id=\"link12\"/> </route>"
                          "       <route src=\"Host1\" dst=\"Host3\"> <link_ctn id=\"link13\"/> </route>"
                          "       <route src=\"Host3\" dst=\"Host3\"> <link_ctn id=\"link23\"/> </route>"
                          "   </zone> "
                          "</platform>";
        FILE *platform_file = fopen(platform_file_path.c_str(), "w");
        fprintf(platform_file, "%s", xml.c_str());
        fclose(platform_file);
    }

    std::string platform_file_path = UNIQUE_TMP_PATH_PREFIX + "platform.xml";

};


/**********************************************************************/
/**  BASIC FUNCTIONALITY SIMULATION TEST                             **/
/**********************************************************************/

class XRootDServiceBasicFunctionalityTestExecutionController : public wrench::ExecutionController {

public:
    XRootDServiceBasicFunctionalityTestExecutionController(XRootDServiceFunctionalTest *test,
                                                           std::string hostname) :
            wrench::ExecutionController(hostname, "test"), test(test) {
    }

private:
    XRootDServiceFunctionalTest *test;

    int main() {

        WRENCH_INFO("Adding a file files to the simulation");
        auto file1 = wrench::Simulation::addFile("file1", 10000);
        auto file2 = wrench::Simulation::addFile("file2", 10000);

        // TODO: The exception message generated by this sucks, we should fix it
        // TODO: I changed it to be more informative
        // this->test->supervisor->createFile(file1);

        // Create a copy file1 on the first child
        //wrench::Simulation::createFile(file1,
        //wrench::FileLocation::LOCATION(this->test->supervisor->getChild(0)->internalStorage));
        this->test->supervisor->getChild(0)->createFile(file1,"/disk100");

        // Read file1 from XRootD
        this->test->supervisor->readFile(file1);

        // Try to read file2 from XRootD although it is nowhere
        // TODO: This should raise some FileNotFound Exception rather
        // that propagating the network timeout. Come to think of it, one should probably
        // set the search timeout as a special property, since the default 30-sec is WAY too long
        // here.
        try {
            this->test->supervisor->readFile(file2);
            throw std::runtime_error("Non extant files should throw exceptions when not found");
        }catch(wrench::ExecutionException &e){


        }


        return 0;
    }
};

TEST_F(XRootDServiceFunctionalTest, BasicFunctionality) {
    DO_TEST_WITH_FORK(do_BasicFunctionality_test);
}

void XRootDServiceFunctionalTest::do_BasicFunctionality_test() {

    // Create and initialize a simulation
    auto simulation = wrench::Simulation::createSimulation();
    int argc = 1;
    char **argv = (char **) calloc(argc, sizeof(char *));
    argv[0] = strdup("unit_test");
    //    argv[1] = strdup("--wrench-full-log");

    simulation->init(&argc, argv);

    // Setting up the platform
    simulation->instantiatePlatform(platform_file_path);

    // Create a XRootD Manager object
    wrench::XRootD::XRootD xrootdManager(simulation,{{wrench::XRootD::Property::CACHE_MAX_LIFETIME,"28800"},{wrench::XRootD::Property::REDUCED_SIMULATION,"true"}},{});

    this->supervisor = xrootdManager.createSupervisor("Host1");

    auto ss2 = xrootdManager.createStorageServer("Host2", "/disk100", {}, {});
    auto ss3 = xrootdManager.createStorageServer("Host3", "/disk100", {}, {});

    supervisor->addChild(ss2);
    supervisor->addChild(ss3);

    // Create an execution controller
    auto controller = simulation->add(new XRootDServiceBasicFunctionalityTestExecutionController(this, "Host1"));


    // Running a "run a single task1" simulation
    ASSERT_NO_THROW(simulation->launch());

    for (int i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
}
