/**
 * Copyright (c) 2017-2021. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <wrench/logging/TerminalOutput.h>
#include <wrench/simulation/Simulation.h>

#include <memory>

#include "wrench/services/compute/batch/batch_schedulers/homegrown/conservative_bf/ConservativeBackfillingBatchScheduler.h"

//#define  PRINT_SCHEDULE 1

WRENCH_LOG_CATEGORY(wrench_core_conservative_bf_batch_scheduler, "Log category for ConservativeBackfillingBatchScheduler");

namespace wrench {

    /**
     * @brief Constructor
     * @param cs: The BatchComputeService for which this scheduler is working
     * @param backfilling_depth: the backfilling depth
     */
    ConservativeBackfillingBatchScheduler::ConservativeBackfillingBatchScheduler(
            BatchComputeService *cs,
            unsigned long backfilling_depth) : HomegrownBatchScheduler(cs) {
        this->schedule = std::make_unique<NodeAvailabilityTimeLine>(cs->total_num_of_nodes);
        this->_backfilling_depth = backfilling_depth;
    }

    /**
     * @brief Method to process a job submission
     * @param batch_job: the newly submitted BatchComputeService job
     */
    void ConservativeBackfillingBatchScheduler::processJobSubmission(std::shared_ptr<BatchJob> batch_job) {
        WRENCH_INFO("Scheduling a new BatchComputeService job, %lu, that needs %lu nodes",
                    batch_job->getJobID(), batch_job->getRequestedNumNodes());

        // Update the time origin
        this->schedule->setTimeOrigin(static_cast<u_int32_t>(Simulation::getCurrentSimulatedDate()));

        // Find its earliest possible start time
        auto est = this->schedule->findEarliestStartTime(batch_job->getRequestedTime(), batch_job->getRequestedNumNodes(), nullptr);
        //        WRENCH_INFO("The Earliest start time is: %u", est);

        // Insert it in the schedule
        this->schedule->add(est, est + batch_job->getRequestedTime(), batch_job);
        batch_job->conservative_bf_start_date = est;
        batch_job->conservative_bf_expected_end_date = est + batch_job->getRequestedTime();
        WRENCH_INFO("Scheduled BatchComputeService job %lu on %lu nodes from time %u to %u",
                    batch_job->getJobID(), batch_job->getRequestedNumNodes(),
                    batch_job->conservative_bf_start_date, batch_job->conservative_bf_expected_end_date);
#ifdef PRINT_SCHEDULE
        this->schedule->print();
#endif
    }

    /**
     * @brief Method to schedule (possibly) the next jobs to be scheduled
     */
    void ConservativeBackfillingBatchScheduler::processQueuedJobs() {
        if (this->cs->batch_queue.empty()) {
            return;
        }

        // Update the time origin
        this->schedule->setTimeOrigin(static_cast<u_int32_t>(Simulation::getCurrentSimulatedDate()));

        // Start  all non-started the jobs in the next slot!
        std::set<std::shared_ptr<BatchJob>> next_jobs = this->schedule->getJobsInFirstSlot();
        if (next_jobs.empty()) {
            this->compactSchedule();
            next_jobs = this->schedule->getJobsInFirstSlot();
        }

        for (auto const &batch_job: next_jobs) {
            // If the job has already been allocated resources, it's already running anyway
            if (not batch_job->resources_allocated.empty()) {
                continue;
            }

            // Get the workflow job associated to the picked BatchComputeService job
            std::shared_ptr<CompoundJob> compound_job = batch_job->getCompoundJob();

            // Find on which resources to actually run the job
            unsigned long cores_per_node_asked_for = batch_job->getRequestedCoresPerNode();
            unsigned long num_nodes_asked_for = batch_job->getRequestedNumNodes();
            unsigned long requested_time = batch_job->getRequestedTime();

            auto resources = this->scheduleOnHosts(num_nodes_asked_for, cores_per_node_asked_for, ComputeService::ALL_RAM);
            if (resources.empty()) {
                // Hmmm... we don't have the resources right now... we should get an update soon....
                return;
                //                throw std::runtime_error("Can't run BatchComputeService job " + std::to_string(batch_job->getJobID()) +  " right now, this shouldn't happen!");
            }

            WRENCH_INFO("Starting BatchComputeService job %lu ", batch_job->getJobID());

            // Remove the job from the BatchComputeService queue
            this->cs->removeJobFromBatchQueue(batch_job);

            // Add it to the running list
            this->cs->running_jobs[batch_job->getCompoundJob()] = batch_job;

            // Start it!
            this->cs->startJob(resources, compound_job, batch_job, num_nodes_asked_for, requested_time,
                               cores_per_node_asked_for);
        }
    }

    /**
     * @brief Method to compact the schedule
     */
    void ConservativeBackfillingBatchScheduler::compactSchedule() {
        WRENCH_INFO("Compacting schedule...");

#ifdef PRINT_SCHEDULE
        //        WRENCH_INFO("BEFORE COMPACTING");
        this->schedule->print();
#endif

        // For each job in the order of the BatchComputeService queue:
        //   - remove the job from the schedule
        //   - re-insert it as early as possible

        // Reset the time origin
        auto now = static_cast<u_int32_t>(Simulation::getCurrentSimulatedDate());
        this->schedule->setTimeOrigin(now);;

        // Go through the BatchComputeService queue
//        for (auto const &batch_job: this->cs->batch_queue) {
        for (unsigned long i=0; i < std::min(this->_backfilling_depth, this->cs->batch_queue.size()); i++) {
            //            WRENCH_INFO("DEALING WITH JOB %lu", batch_job->getJobID());
            auto batch_job  = this->cs->batch_queue.at(i);

            // Remove the job from the schedule
            //            WRENCH_INFO("REMOVING IT FROM SCHEDULE");
            this->schedule->remove(batch_job->conservative_bf_start_date, batch_job->conservative_bf_expected_end_date + 100, batch_job);
            //            this->schedule->print();

            // Find the earliest start time
            //            WRENCH_INFO("FINDING THE EARLIEST START TIME");
            auto est = this->schedule->findEarliestStartTime(batch_job->getRequestedTime(), batch_job->getRequestedNumNodes(), nullptr);
            //            WRENCH_INFO("EARLIEST START TIME FOR IT: %u", est);
            // Insert it in the schedule
            this->schedule->add(est, est + batch_job->getRequestedTime(), batch_job);
            //            WRENCH_INFO("RE-INSERTED THERE!");
            //            this->schedule->print();

            batch_job->conservative_bf_start_date = est;
            batch_job->conservative_bf_expected_end_date = est + batch_job->getRequestedTime();
        }


#if 0
        // OLD IMPLEMENTATION THAT RECONSTRUCTS THE SCHEDULE FROM SCRATCH

        // Clear the schedule
        this->schedule->clear();

        // Reset the time origin
        auto now = (u_int32_t)Simulation::getCurrentSimulatedDate();
        this->schedule->setTimeOrigin(now);

        // Add the running job time slots
        for (auto  const &batch_job : this->cs->running_jobs) {
            this->schedule->add(now, batch_job->conservative_bf_expected_end_date, batch_job);
        }

        // Add in all other jobs as early as possible in BatchComputeService queue order
        for (auto const &batch_job : this->cs->batch_queue) {
            auto est = this->schedule->findEarliestStartTime(batch_job->getRequestedTime(), batch_job->getRequestedNumNodes());
            // Insert it in the schedule
            this->schedule->add(est, est + batch_job->getRequestedTime(), batch_job);
            batch_job->conservative_bf_start_date = est;
            batch_job->conservative_bf_expected_end_date = est + batch_job->getRequestedTime();
        }
#endif

#ifdef PRINT_SCHEDULE
        WRENCH_INFO("AFTER COMPACTING");
        this->schedule->print();
#endif
    }

    /**
     * @brief Method to process a job completion
     * @param batch_job: the job that completed
     */
    void ConservativeBackfillingBatchScheduler::processJobCompletion(std::shared_ptr<BatchJob> batch_job) {
        WRENCH_INFO("Notified of completion of BatchComputeService job, %lu", batch_job->getJobID());

        auto now = static_cast<u_int32_t>(Simulation::getCurrentSimulatedDate());
        this->schedule->setTimeOrigin(now);
        this->schedule->remove(now, batch_job->conservative_bf_expected_end_date + 100, batch_job);

#ifdef PRINT_SCHEDULE
        this->schedule->print();
#endif

        if (now < batch_job->conservative_bf_expected_end_date) {
            compactSchedule();
        }
    }

    /**
    * @brief Method to process a job termination
    * @param batch_job: the job that was terminated
    */
    void ConservativeBackfillingBatchScheduler::processJobTermination(std::shared_ptr<BatchJob> batch_job) {
        // Just like a job Completion to me!
        this->processJobCompletion(batch_job);
    }

    /**
    * @brief Method to process a job failure
    * @param batch_job: the job that failed
    */
    void ConservativeBackfillingBatchScheduler::processJobFailure(std::shared_ptr<BatchJob> batch_job) {
        // Just like a job Completion to me!
        this->processJobCompletion(batch_job);
    }

    /**
     * @brief Method to figure out on which actual resources a job could be scheduled right now
     * @param num_nodes: number of nodes
     * @param cores_per_node: number of cores per node
     * @param ram_per_node: amount of RAM
     * @return a host:<core,RAM> map
     *
     */
    std::map<simgrid::s4u::Host *, std::tuple<unsigned long, sg_size_t>>
    ConservativeBackfillingBatchScheduler::scheduleOnHosts(unsigned long num_nodes, unsigned long cores_per_node, sg_size_t ram_per_node) {
        if (ram_per_node == ComputeService::ALL_RAM) {
            ram_per_node = S4U_Simulation::getHostMemoryCapacity(cs->available_nodes_to_cores.begin()->first);
        }
        if (cores_per_node == ComputeService::ALL_CORES) {
            cores_per_node = cs->available_nodes_to_cores.begin()->first->get_core_count();
        }

        if (ram_per_node > S4U_Simulation::getHostMemoryCapacity(cs->available_nodes_to_cores.begin()->first)) {
            throw std::runtime_error("CONSERVATIVE_BFBatchScheduler::scheduleOnHosts(): Asking for too much RAM per host");
        }
        if (num_nodes > cs->available_nodes_to_cores.size()) {
            throw std::runtime_error("CONSERVATIVE_BFBatchScheduler::scheduleOnHosts(): Asking for too many hosts");
        }
        if (cores_per_node > static_cast<unsigned long>(cs->available_nodes_to_cores.begin()->first->get_core_count())) {
            throw std::runtime_error("CONSERVATIVE_BFBatchScheduler::scheduleOnHosts(): Asking for too many cores per host (asking  for " +
                                     std::to_string(cores_per_node) + " but hosts have " +
                                     std::to_string(cs->available_nodes_to_cores.begin()->first->get_core_count()) + "cores)");
        }

        // IMPORTANT: We always give all cores to a job on a node!
        cores_per_node = cs->available_nodes_to_cores.begin()->first->get_core_count();

        return HomegrownBatchScheduler::selectHostsFirstFit(cs, num_nodes, cores_per_node, ram_per_node);
    }

    /**
     * @brief Method to obtain start time estimates
     * @param set_of_jobs: a set of job specs
     * @return map of estimates
     */
    std::map<std::string, double> ConservativeBackfillingBatchScheduler::getStartTimeEstimates(
            std::set<std::tuple<std::string, unsigned long, unsigned long, sg_size_t>> set_of_jobs) {
        std::map<std::string, double> to_return;

        for (auto const &j: set_of_jobs) {
            const std::string &id = std::get<0>(j);
            u_int64_t num_nodes = std::get<1>(j);
            u_int64_t num_cores_per_host = this->cs->num_cores_per_node;// Ignore this one. Assume all  cores!
            if (std::get<3>(j) > UINT32_MAX) {
                throw std::runtime_error("ConservativeBackfillingBatchScheduler::getStartTimeEstimates(): job duration too large");
            }
            auto duration = static_cast<u_int32_t>(std::get<3>(j));

            auto est = this->schedule->findEarliestStartTime(duration, num_nodes, nullptr);
            if (est < UINT32_MAX) {
                to_return[id] = static_cast<double>(est);
            } else {
                to_return[id] = -1.0;
            }
        }
        return to_return;
    }

}// namespace wrench
