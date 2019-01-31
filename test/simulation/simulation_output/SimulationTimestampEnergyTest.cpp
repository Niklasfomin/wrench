#include <gtest/gtest.h>
#include <wrench-dev.h>

#include "../../include/TestWithFork.h"
#include "../../include/UniqueTmpPathPrefix.h"

class SimulationTimestampEnergyTest: public ::testing::Test {
public:
    wrench::WorkflowTask *task1 = nullptr;
    std::unique_ptr<wrench::Workflow> workflow;

    wrench::ComputeService *compute_service = nullptr;

    void do_SimulationTimestampPstateSet_test();
    //void do_SimulationTimestampEnergyConsumption_test();

protected:

    SimulationTimestampEnergyTest() : workflow(std::unique_ptr<wrench::Workflow>(new wrench::Workflow())) {
        // Create a two-host 1-core platform file along with different pstates
        std::string xml = "<?xml version='1.0'?>"
                          "<!DOCTYPE platform SYSTEM \"http://simgrid.gforge.inria.fr/simgrid/simgrid.dtd\">"
                          "<platform version=\"4.1\">"
                          "<zone id=\"AS0\" routing=\"Full\">"

                          "<host id=\"host1\" speed=\"100.0Mf,50.0Mf,20.0Mf\" pstate=\"0\" core=\"1\" >"
                          "<prop id=\"watt_per_state\" value=\"100.0:200.0, 93.0:170.0, 90.0:150.0\" />"
                          "<prop id=\"watt_off\" value=\"10\" />"
                          "</host>"

                          "</zone>"
                          "</platform>";

        FILE *platform_file = fopen(platform_file_path.c_str(), "w");
        fprintf(platform_file, "%s", xml.c_str());
        fclose(platform_file);
    }

    std::string platform_file_path = UNIQUE_TMP_PATH_PREFIX + "platform.xml";

};


/**********************************************************************/
/**            SimulationTimestampPstateSetTest                      **/
/**********************************************************************/

/*
 * Testing the basic functionality of the SimulationTimestampPstateSet class.
 * This test assures that whenever the energy plugin is activated, and
 * setPstate() is called, a timestamp is recorded and added to the simulation
 * output.
 */
class SimulationTimestampPstateSetTestWMS : public wrench::WMS {
public:
    SimulationTimestampPstateSetTestWMS(SimulationTimestampEnergyTest *test,
            std::string &hostname) :
                wrench::WMS(nullptr, nullptr, {}, {}, {}, nullptr, hostname, "test") {
        this->test = test;
    }

private:
    SimulationTimestampEnergyTest *test;

    int main() {
        wrench::S4U_Simulation::sleep(10.0);
        this->simulation->setPstate(this->getHostname(), 1);
        wrench::S4U_Simulation::sleep(10.0);
        this->simulation->setPstate(this->getHostname(), 2);

        return 0;
    }

};

TEST_F(SimulationTimestampEnergyTest, SimulationTimestampPstateSetTest) {
    DO_TEST_WITH_FORK(do_SimulationTimestampPstateSet_test);
}

void SimulationTimestampEnergyTest::do_SimulationTimestampPstateSet_test() {
    auto simulation = new wrench::Simulation();
    int argc = 2;
    auto argv = (char **)calloc(argc, sizeof(char *));
    argv[0] = strdup("simulation_timestamp_pstate_set_test");
    argv[1] = strdup("--activate-energy");

    EXPECT_NO_THROW(simulation->init(&argc, argv));

    EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

    // get the single host
    std::string host = simulation->getHostnameList()[0];

    wrench::WMS *wms = nullptr;
    EXPECT_NO_THROW(wms = simulation->add(
            new SimulationTimestampPstateSetTestWMS(
                    this, host
                    )
            ));

    EXPECT_NO_THROW(wms->addWorkflow(workflow.get()));

    EXPECT_NO_THROW(simulation->launch());

    // Check constructors for SimulationTimestampPstateSet timestamps
    EXPECT_THROW(new wrench::SimulationTimestampPstateSet("", 0), std::invalid_argument);

    // Check that the expected SimulationTimestampPstateSet timestamps have been added to simulation output
    auto set_pstate_timestamps = simulation->getOutput().getTrace<wrench::SimulationTimestampPstateSet>();
    ASSERT_EQ(3, set_pstate_timestamps.size());

    // pstate timestamp added prior to the simulation starting
    auto ts1 = set_pstate_timestamps[0];
    ASSERT_EQ(0, ts1->getDate());
    ASSERT_EQ(0, ts1->getContent()->getPstate());
    ASSERT_EQ("host1", ts1->getContent()->getHostname());

    // pstate timestamp added from wms first call of setPstate()
    auto ts2 = set_pstate_timestamps[1];
    ASSERT_EQ(10, ts2->getDate());
    ASSERT_EQ(1, ts2->getContent()->getPstate());
    ASSERT_EQ("host1", ts2->getContent()->getHostname());

    // pstate timestamp added from wms second call of setPstate()
    auto ts3 = set_pstate_timestamps[2];
    ASSERT_EQ(20, ts3->getDate());
    ASSERT_EQ(2, ts3->getContent()->getPstate());
    ASSERT_EQ("host1", ts3->getContent()->getHostname());

    delete simulation;
    free(argv[0]);
    free(argv[1]);
    free(argv);
}
