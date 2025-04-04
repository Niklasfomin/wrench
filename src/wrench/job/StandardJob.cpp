/**
 * Copyright (c) 2017-2021. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <set>
#include <wrench-dev.h>
#include <wrench/workflow/Workflow.h>
#include <wrench/job/StandardJob.h>
#include <wrench/services/helper_services/action_execution_service/ActionExecutionService.h>

WRENCH_LOG_CATEGORY(wrench_core_standard_job, "Log category for StandardJob");

namespace wrench {

    /**
     * @brief Constructor
     *
     * @param job_manager: the job manager that creates this job
     *
     * @param tasks: the tasks in the job (which must be either READY, or children of COMPLETED tasks or
     *                                   of tasks also included in the standard job)
     * @param file_locations: a map that specifies locations where input/output files should be read/written
     * @param pre_file_copies: a vector of tuples that specify which file copy operations should be completed
     *                         before task executions begin
     * @param post_file_copies: a vector of tuples that specify which file copy operations should be completed
     *                         after task executions end
     * @param cleanup_file_deletions: a vector of tuples that specify which file copies should be removed from which
     *                         locations. This will happen regardless of whether the job succeeds or fails
     *
     */
    StandardJob::StandardJob(std::shared_ptr<JobManager> job_manager,
                             std::vector<std::shared_ptr<WorkflowTask>> tasks,
                             std::map<std::shared_ptr<DataFile>, std::vector<std::shared_ptr<FileLocation>>> &file_locations,
                             std::vector<std::tuple<std::shared_ptr<FileLocation>, std::shared_ptr<FileLocation>>> &pre_file_copies,
                             std::vector<std::tuple<std::shared_ptr<FileLocation>, std::shared_ptr<FileLocation>>> &post_file_copies,
                             std::vector<std::shared_ptr<FileLocation>> &cleanup_file_deletions)
        : Job("", std::move(job_manager)),
          file_locations(file_locations),
          pre_file_copies(pre_file_copies),
          post_file_copies(post_file_copies),
          cleanup_file_deletions(cleanup_file_deletions),
          state(StandardJob::State::NOT_SUBMITTED) {
        // Check that this is a ready sub-graph
        for (const auto &t: tasks) {
            if (t->getState() != WorkflowTask::State::READY) {
                std::vector<std::shared_ptr<WorkflowTask>> parents = t->getWorkflow()->getTaskParents(t);
                for (const auto &parent: parents) {
                    if (parent->getState() != WorkflowTask::State::COMPLETED) {
                        if (std::find(tasks.begin(), tasks.end(), parent) == tasks.end()) {
                            throw std::invalid_argument("StandardJob::StandardJob(): Task '" + t->getID() +
                                                        "' has non-completed parents not included in the job");
                        }
                    }
                }
            }
        }

        for (const auto &t: tasks) {
            this->tasks.push_back(t);
            t->setJob(this);
            this->total_flops += t->getFlops();
        }
        //        this->workflow = workflow;
        this->name = "standard_job_" + std::to_string(Job::getNewUniqueNumber());
        this->total_flops = 0.0;
    }

    /**
     * @brief Returns the minimum number of cores required to run the job (i.e., at least
     *        one task in the job cannot run if fewer cores than this minimum are available)
     * @return the number of cores
     */
    unsigned long StandardJob::getMinimumRequiredNumCores() const {
        unsigned long max_min_num_cores = 1;
        for (const auto &t: tasks) {
            max_min_num_cores = std::max<unsigned long>(max_min_num_cores, t->getMinNumCores());
        }
        return max_min_num_cores;
    }

    /**
     * @brief Returns the minimum RAM capacity required to run the job (i.e., at least
     *        one task in the job cannot run if less ram than this minimum is available)
     * @return the number of cores
     */
    sg_size_t StandardJob::getMinimumRequiredMemory() const {
        sg_size_t max_ram = 0;
        for (auto const &t: tasks) {
            max_ram = std::max<sg_size_t>(max_ram, t->getMemoryRequirement());
        }
        return max_ram;
    }


    /**
     * @brief Get the number of tasks in the job
     *
     * @return the number of tasks
     */
    unsigned long StandardJob::getNumTasks() const {
        return this->tasks.size();
    }

    /**
     * @brief Get the number of completed tasks in the job
     *
     * @return the number of completed tasks
     */
    unsigned long StandardJob::getNumCompletedTasks() const {
        unsigned long count = 0;
        for (auto const &t: this->tasks) {
            if (t->getState() == WorkflowTask::State::COMPLETED) {
                count++;
            }
        }
        return count;
    }

    /**
     * @brief Get the workflow tasks in the job
     *
     * @return a vector of workflow tasks
     */
    std::vector<std::shared_ptr<WorkflowTask>> StandardJob::getTasks() const {
        return this->tasks;
    }

    /**
     * @brief Get the file location map for the job
     *
     * @return a map of files to storage services
     */
    std::map<std::shared_ptr<DataFile>, std::vector<std::shared_ptr<FileLocation>>> StandardJob::getFileLocations() const {
        return this->file_locations;
    }

    /**
     * @brief Get the state of the standard job
     * @return the state
     */
    StandardJob::State StandardJob::getState() const {
        return this->state;
    }

    /**
     * @brief get the job's pre-overhead
     * @return a number o seconds
     */
    double StandardJob::getPreJobOverheadInSeconds() const {
        return this->pre_overhead;
    }

    /**
    * @brief get the job's post-overhead
    * @return a number o seconds
    */
    double StandardJob::getPostJobOverheadInSeconds() const {
        return this->post_overhead;
    }

    /**
    * @brief sets the job's pre-overhead
    * @param overhead: the overhead in seconds
    */
    void StandardJob::setPreJobOverheadInSeconds(double overhead) {
        this->pre_overhead = overhead;
    }

    /**
    * @brief sets the job's post-overhead
    * @param overhead: the overhead in seconds
    */
    void StandardJob::setPostJobOverheadInSeconds(double overhead) {
        this->post_overhead = overhead;
    }

    /**
     * @brief Instantiate a compound job
     */
    void StandardJob::createUnderlyingCompoundJob(const std::shared_ptr<ComputeService> &compute_service) {
        this->compound_job = nullptr;
        this->pre_overhead_action = nullptr;
        this->post_overhead_action = nullptr;
        this->pre_file_copy_actions.clear();
        this->task_file_read_actions.clear();
        this->task_compute_actions.clear();
        this->task_file_write_actions.clear();
        this->post_file_copy_actions.clear();
        this->cleanup_actions.clear();
        this->scratch_cleanup = nullptr;

        auto cjob = this->job_manager->createCompoundJob("cjob_for_" + this->getName());

        // Create pre- and post-overhead work units
        if (this->getPreJobOverheadInSeconds() > 0.0) {
            pre_overhead_action = cjob->addSleepAction("", this->getPreJobOverheadInSeconds());
        }

        if (this->getPostJobOverheadInSeconds() > 0.0) {
            post_overhead_action = cjob->addSleepAction("", this->getPostJobOverheadInSeconds());
        }

        // Create the pre file copy actions
        for (auto const &pfc: this->pre_file_copies) {
            auto src_location = std::get<0>(pfc);
            auto dst_location = std::get<1>(pfc);
            //            if (src_location->isScratch()) {
            //                src_location = FileLocation::LOCATION(compute_service->getScratch(), src_location->getFile());
            //            }
            //            if (dst_location->isScratch()) {
            //                dst_location = FileLocation::LOCATION(compute_service->getScratch(), dst_location->getFile());
            //            }

            pre_file_copy_actions.push_back(cjob->addFileCopyAction("", src_location, dst_location));
        }

        // Create the post file copy actions
        for (auto const &pfc: this->post_file_copies) {
            auto src_location = std::get<0>(pfc);
            auto dst_location = std::get<1>(pfc);
            //            if (src_location->isScratch()) {
            //                src_location = FileLocation::LOCATION(compute_service->getScratch(), src_location->getFile());
            //            }
            //            if (dst_location->isScratch()) {
            //                dst_location = FileLocation::LOCATION(compute_service->getScratch(), dst_location->getFile());
            //            }

            post_file_copy_actions.push_back(cjob->addFileCopyAction("", src_location, dst_location));
        }

        // Create the file cleanup actions
        for (auto const &fc: this->cleanup_file_deletions) {
            cleanup_actions.push_back(cjob->addFileDeleteAction("", fc));
        }


        // Create the task actions
        for (auto const &task: this->tasks) {
            // Create the Compute Action
            auto compute_action = cjob->addComputeAction("task_" + task->getID(), task->getFlops(), task->getMemoryRequirement(),
                                                         task->getMinNumCores(), task->getMaxNumCores(), task->getParallelModel());
            task_compute_actions[task] = compute_action;

            // Create the Task File Read Actions
            task_file_read_actions[task] = {};
            for (auto const &f: task->getInputFiles()) {
                std::shared_ptr<Action> fread_action;
                if (this->file_locations.find(f) != this->file_locations.end()) {
                    //                    std::vector<std::shared_ptr<FileLocation>> fixed_locations = this->file_locations[f];
                    //                    for (auto const &loc: this->file_locations[f]) {
                    //                        fixed_locations.push_back(loc);
                    //                    }
                    fread_action = cjob->addFileReadAction("", this->file_locations[f]);
                } else {
                    fread_action = cjob->addFileReadAction("", FileLocation::SCRATCH(f));
                }
                task_file_read_actions[task].push_back(fread_action);
                cjob->addActionDependency(fread_action, compute_action);
            }
            task_file_write_actions[task] = {};
            for (auto const &f: task->getOutputFiles()) {
                std::shared_ptr<Action> fwrite_action;
                if (this->file_locations.find(f) != this->file_locations.end()) {
                    std::vector<std::shared_ptr<FileLocation>> fixed_locations = this->file_locations[f];
                    //                    for (auto const &loc: this->file_locations[f]) {
                    //                        fixed_locations.push_back(loc);
                    //                    }
                    if (fixed_locations.size() > 1) {
                        throw std::runtime_error("StandardJob::createUnderlyingCompoundJob(): Internal WRENCH error - "
                                                 "there should be a single location for a file write action");
                    }
                    fwrite_action = cjob->addFileWriteAction("", this->file_locations[f].at(0));
                } else {
                    fwrite_action = cjob->addFileWriteAction("", FileLocation::SCRATCH(f));
                }
                task_file_write_actions[task].push_back(fwrite_action);
                cjob->addActionDependency(compute_action, fwrite_action);
            }
        }

        // Determine whether the scratch space needs to be cleaned
        //        std::cerr << "DETERMINING IS SCRATCH CLEAN IS NEEDED\n";
        //        bool need_scratch_clean = false;
        //        for (auto const &task: this->tasks) {
        //            for (auto const &f: task->getInputFiles()) {
        //                if (this->file_locations.find(f) == this->file_locations.end()) {
        //                    need_scratch_clean = true;
        //                    break;
        //                }
        //                for (const auto &fl : this->file_locations[f]) {
        //                    if (fl->isScratch()) {
        //                        need_scratch_clean = true;
        //                        break;
        //                    }
        //                }
        //                if (need_sc)
        //            }
        //            if (need_scratch_clean) {
        //                break;
        //            }
        //            for (auto const &f: task->getOutputFiles()) {
        //                if (this->file_locations.find(f) == this->file_locations.end()) {
        //                    need_scratch_clean = true;
        //                    break;
        //                }
        //            }
        //            if (need_scratch_clean) {
        //                break;
        //            }
        //        }

        // Create the scratch clean up actions
        //        std::cerr << "NEED SCRATCH CLEAN = " << need_scratch_clean << "\n";
        // Does the lambda capture of cjob_file_locations work?
        std::weak_ptr<CompoundJob> weak_cjob = cjob;
        auto lambda_execute = [weak_cjob](const std::shared_ptr<wrench::ActionExecutor> &action_executor) {
            auto cs = std::dynamic_pointer_cast<ComputeService>(action_executor->getActionExecutionService()->getParentService());
            auto scratch = cs->getScratch();
            if (scratch) {
                auto cjob = weak_cjob.lock();
                scratch->removeDirectory(scratch->getBaseRootPath() + cjob->getName());
            }
        };
        auto lambda_terminate = [](const std::shared_ptr<wrench::ActionExecutor> &action_executor) {};
        scratch_cleanup = cjob->addCustomAction("", 0, 0, lambda_execute, lambda_terminate);


        // Add all inter-task dependencies
        for (auto const &parent_task: this->tasks) {
            for (auto const &child_task: parent_task->getChildren()) {
                if (task_compute_actions.find(child_task) == task_compute_actions.end()) {
                    continue;
                }
                std::vector<std::shared_ptr<Action>> parent_actions;
                if (not task_file_write_actions[parent_task].empty()) {
                    parent_actions = task_file_write_actions[parent_task];
                } else {
                    parent_actions = {task_compute_actions[parent_task]};
                }
                std::vector<std::shared_ptr<Action>> child_actions;
                if (not task_file_read_actions[child_task].empty()) {
                    child_actions = task_file_read_actions[child_task];
                } else {
                    child_actions = {task_compute_actions[child_task]};
                }
                for (auto const &parent_action: parent_actions) {
                    for (auto const &child_action: child_actions) {
                        cjob->addActionDependency(parent_action, child_action);
                    }
                }
            }
        }

        // Create dummy tasks
        std::shared_ptr<Action> pre_overhead_to_pre_file_copies = cjob->addSleepAction("", 0);
        std::shared_ptr<Action> pre_file_copies_to_tasks = cjob->addSleepAction("", 0);
        std::shared_ptr<Action> tasks_to_post_file_copies = cjob->addSleepAction("", 0);
        std::shared_ptr<Action> tasks_post_file_copies_to_cleanup = cjob->addSleepAction("", 0);
        std::shared_ptr<Action> cleanup_to_post_overhead = cjob->addSleepAction("", 0);
        cjob->addActionDependency(pre_overhead_to_pre_file_copies, pre_file_copies_to_tasks);
        cjob->addActionDependency(pre_file_copies_to_tasks, tasks_to_post_file_copies);
        cjob->addActionDependency(tasks_to_post_file_copies, tasks_post_file_copies_to_cleanup);
        cjob->addActionDependency(tasks_post_file_copies_to_cleanup, cleanup_to_post_overhead);

        // Add all dependencies, using the dummy tasks to help
        if (pre_overhead_action != nullptr) {
            cjob->addActionDependency(pre_overhead_action, pre_overhead_to_pre_file_copies);
        }
        if (not pre_file_copy_actions.empty()) {
            for (auto const &pfca: pre_file_copy_actions) {
                cjob->addActionDependency(pre_overhead_to_pre_file_copies, pfca);
                cjob->addActionDependency(pfca, pre_file_copies_to_tasks);
            }
        }

        if (not task_compute_actions.empty()) {
            for (auto const &tca: task_compute_actions) {
                auto task = tca.first;
                if (not task_file_read_actions[task].empty()) {
                    for (auto const &tfra: task_file_read_actions[task]) {
                        cjob->addActionDependency(pre_file_copies_to_tasks, tfra);
                    }
                } else {
                    cjob->addActionDependency(pre_file_copies_to_tasks, tca.second);
                }
                if (not task_file_write_actions[task].empty()) {
                    for (auto const &tfwa: task_file_write_actions[task]) {
                        cjob->addActionDependency(tfwa, tasks_to_post_file_copies);
                    }
                } else {
                    cjob->addActionDependency(tca.second, tasks_to_post_file_copies);
                }
            }
        }

        if (not post_file_copy_actions.empty()) {
            for (auto const &pfca: post_file_copy_actions) {
                cjob->addActionDependency(tasks_to_post_file_copies, pfca);
                cjob->addActionDependency(pfca, tasks_post_file_copies_to_cleanup);
            }
        }

        if (not cleanup_actions.empty()) {
            for (auto const &ca: cleanup_actions) {
                cjob->addActionDependency(tasks_post_file_copies_to_cleanup, ca);
                cjob->addActionDependency(ca, cleanup_to_post_overhead);
            }
        }

        if (post_overhead_action != nullptr) {
            cjob->addActionDependency(cleanup_to_post_overhead, post_overhead_action);
            cjob->addActionDependency(post_overhead_action, scratch_cleanup);
        } else {
            cjob->addActionDependency(cleanup_to_post_overhead, scratch_cleanup);
        }

        // Use the dummy tasks for "easy" dependencies and remove the dummies
        std::vector<std::shared_ptr<Action>> dummies = {pre_overhead_to_pre_file_copies, pre_file_copies_to_tasks, tasks_to_post_file_copies, tasks_post_file_copies_to_cleanup, cleanup_to_post_overhead};
        for (auto &dummy: dummies) {
            // propagate dependencies
            for (auto const &parent_action: dummy->getParents()) {
                for (auto const &child_action: dummy->getChildren()) {
                    cjob->addActionDependency(parent_action, child_action);
                }
            }
            // remove the dummy
            cjob->removeAction(dummy);
        }

        //            cjob->printActionDependencies();
        this->compound_job = std::move(cjob);
    }

    void StandardJob::analyzeActions(const std::vector<std::shared_ptr<Action>> &actions,
                                     bool *at_least_one_failed,
                                     bool *at_least_one_killed,
                                     std::shared_ptr<FailureCause> *failure_cause,
                                     double *earliest_start_date,
                                     double *latest_end_date,
                                     double *earliest_failure_date) {
        *at_least_one_failed = false;
        *at_least_one_killed = false;
        *failure_cause = nullptr;
        *earliest_start_date = -1.0;
        *latest_end_date = -1.0;
        *earliest_failure_date = -1.0;

        for (const auto &action: actions) {
            // Set the dates
            if ((*earliest_start_date == -1.0) or ((action->getStartDate() < *earliest_start_date) and (action->getStartDate() != -1.0))) {
                *earliest_start_date = action->getStartDate();
            }
            if ((*latest_end_date == -1.0) or ((action->getEndDate() > *latest_end_date) and (action->getEndDate() != -1.0))) {
                *latest_end_date = action->getEndDate();
            }

            if (action->getState() == Action::State::FAILED || action->getState() == Action::State::KILLED) {
                *at_least_one_failed = true;
                if (not *failure_cause) {
                    *failure_cause = action->getFailureCause();
                }
                if ((*earliest_failure_date == -1.0) or
                    ((*earliest_failure_date > action->getEndDate()) and (action->getEndDate() != -1.0))) {
                    *earliest_failure_date = action->getEndDate();
                }
            }
        }
    }

    /**
 * @brief Compute all task updates based on the state of the underlying compound job (also updates timing information and other task information)
 * @param state_changes: the set of task state changes to apply
 * @param failure_count_increments: the set ot task failure count increments to apply
 * @param job_failure_cause: the job failure cause, if any
 * @param simulation: the simulation (to add timestamps!)
 */
    void StandardJob::processCompoundJobOutcome(std::map<std::shared_ptr<WorkflowTask>, WorkflowTask::State> &state_changes,
                                                std::set<std::shared_ptr<WorkflowTask>> &failure_count_increments,
                                                std::shared_ptr<FailureCause> &job_failure_cause,
                                                Simulation *simulation) {
#ifdef WRENCH_INTERNAL_EXCEPTIONS
        switch (this->state) {
            case StandardJob::State::PENDING:
            case StandardJob::State::RUNNING:
                throw std::runtime_error("StandardJob::processCompoundJobOutcome(): Cannot be called on a RUNNING/PENDING job");
            default:
                break;
        }
#endif

        // At this point all tasks are pending, so no matter what we need to change all states
        // So we provisionally make them all NOT_READY right now, which we may overwrite with
        // COMPLETED, and then and the level above may turn some of the NOT_READY into READY.
        for (auto const &t: this->tasks) {
            state_changes[t] = WorkflowTask::State::NOT_READY;
        }

        job_failure_cause = nullptr;

        //        for (auto const &a: this->compound_job->getActions()) {
        //            std::cerr << "ACTION " << a->getName() << ": STATE=" << Action::stateToString(a->getState()) << "\n";
        //            if (a->getFailureCause()) {
        //                std::cerr << "ACTION " << a->getName() << ": " << a->getFailureCause()->toString() << "\n";
        //            } else {
        //                std::cerr << "ACTION " << a->getName() << ": NO FAILURE CAUSE\n";
        //            }
        //        }

        /*
         * Look at Overhead action
         */
        if (this->pre_overhead_action) {
            if (this->pre_overhead_action->getState() == Action::State::KILLED) {
                job_failure_cause = std::make_shared<JobKilled>(this->getSharedPtr());
                for (auto const &t: this->tasks) {
                    failure_count_increments.insert(t);
                }
                return;
            } else if (this->pre_overhead_action->getState() == Action::State::FAILED) {
                job_failure_cause = this->pre_overhead_action->getFailureCause();
                for (auto const &t: this->tasks) {
                    failure_count_increments.insert(t);
                }
                return;
            }
        }

        /*
         * Look at Pre-File copy actions
         */
        if (not this->pre_file_copy_actions.empty()) {
            bool at_least_one_failed, at_least_one_killed;
            std::shared_ptr<FailureCause> failure_cause;
            double earliest_start_date, latest_end_date, earliest_failure_date;
            this->analyzeActions(this->pre_file_copy_actions,
                                 &at_least_one_failed,
                                 &at_least_one_killed,
                                 &failure_cause,
                                 &earliest_start_date,
                                 &latest_end_date,
                                 &earliest_failure_date);
            if (at_least_one_killed) {
                job_failure_cause = std::make_shared<JobKilled>(this->getSharedPtr());
                for (auto const &t: this->tasks) {
                    failure_count_increments.insert(t);
                }
                return;
            } else if (at_least_one_failed) {
                job_failure_cause = failure_cause;
                for (auto const &t: this->tasks) {
                    failure_count_increments.insert(t);
                }
                return;
            }
        }

        /*
         * Look at all the tasks
         */
        for (auto &t: this->tasks) {
            // Set a provisional start date and end date
            t->setStartDate(-1.0);
            t->setEndDate(-1.0);

            std::string execution_host = this->task_compute_actions[t]->getExecutionHistory().top().execution_host;

            if (execution_host.empty()) {
                for (const auto &a: this->task_file_read_actions[t]) {
                    if (!a->getExecutionHistory().top().execution_host.empty()) {
                        execution_host = a->getExecutionHistory().top().execution_host;
                        break;
                    }
                }
            }

            t->setExecutionHost(execution_host);

            /*
             * Look at file-read actions
             */

            // Create fileread time stamps
            for (auto const &a: this->task_file_read_actions[t]) {
                auto fra = std::dynamic_pointer_cast<FileReadAction>(a);
                auto fra_file = fra->getFile();
                switch (a->getState()) {
                    case Action::NOT_READY:
                    case Action::READY:
                        break;
                    case Action::STARTED:
                        simulation->getOutput().addTimestampFileReadStart(fra->getStartDate(), fra_file, fra->getUsedFileLocation(), fra->getUsedFileLocation()->getStorageService(), t);
                        break;
                    case Action::COMPLETED:
                        simulation->getOutput().addTimestampFileReadStart(fra->getStartDate(), fra_file, fra->getUsedFileLocation(), fra->getUsedFileLocation()->getStorageService(), t);
                        simulation->getOutput().addTimestampFileReadCompletion(fra->getEndDate(), fra_file, fra->getUsedFileLocation(), fra->getUsedFileLocation()->getStorageService(), t);
                        break;
                    case Action::KILLED:
                    case Action::FAILED:
                        simulation->getOutput().addTimestampFileReadStart(fra->getStartDate(), fra_file, fra->getUsedFileLocation(), fra->getUsedFileLocation()->getStorageService(), t);
                        simulation->getOutput().addTimestampFileReadFailure(fra->getEndDate(), fra_file, fra->getUsedFileLocation(), fra->getUsedFileLocation()->getStorageService(), t);
                        break;
                }
            }

            bool at_least_one_failed, at_least_one_killed;
            std::shared_ptr<FailureCause> failure_cause;
            double earliest_start_date, latest_end_date, earliest_failure_date;
            this->analyzeActions(this->task_file_read_actions[t],
                                 &at_least_one_failed,
                                 &at_least_one_killed,
                                 &failure_cause,
                                 &earliest_start_date,
                                 &latest_end_date,
                                 &earliest_failure_date);

            if (at_least_one_failed or at_least_one_killed) {
                t->updateStartDate(earliest_start_date);

                t->setFailureDate(earliest_failure_date);
                t->setReadInputStartDate(earliest_start_date);
                failure_count_increments.insert(t);
                state_changes[t] = WorkflowTask::State::READY;// This may be changed to NOT_READY later based on other tasks
                simulation->getOutput().addTimestampTaskStart(earliest_start_date, t);
                if (at_least_one_killed) {
                    job_failure_cause = std::make_shared<JobKilled>(this->getSharedPtr());
                    t->setTerminationDate(earliest_failure_date);
                    simulation->getOutput().addTimestampTaskTermination(earliest_failure_date, t);
                } else if (at_least_one_failed) {
                    if (job_failure_cause == nullptr) job_failure_cause = failure_cause;

                    simulation->getOutput().addTimestampTaskFailure(earliest_failure_date, t);
                }
                continue;
            }

            t->updateStartDate(earliest_start_date);// could be -1.0 if there were no input, but will be updated below
            t->setReadInputStartDate(earliest_start_date);
            t->setReadInputEndDate(latest_end_date);

            /*
             * Look at the compute action
             */
            // Look at the compute action
            auto compute_action = this->task_compute_actions[t];
            t->setComputationStartDate(compute_action->getStartDate());

            simulation->getOutput().addTimestampTaskStart(compute_action->getStartDate(), t);

            t->setNumCoresAllocated(compute_action->getExecutionHistory().top().num_cores_allocated);
            if (t->getStartDate() == -1.0) {
                t->updateStartDate(t->getComputationStartDate());
            }

            if (compute_action->getState() != Action::State::COMPLETED) {
                if (not job_failure_cause) job_failure_cause = compute_action->getFailureCause();
                if (compute_action->getState() == Action::State::KILLED) {
                    t->setTerminationDate(compute_action->getEndDate());
                    simulation->getOutput().addTimestampTaskTermination(compute_action->getEndDate(), t);
                } else {
                    simulation->getOutput().addTimestampTaskFailure(compute_action->getEndDate(), t);
                    t->setFailureDate(compute_action->getEndDate());
                }
                failure_count_increments.insert(t);
                state_changes[t] = WorkflowTask::State::READY;// This may be changed to NOT_READY later
                continue;
            }

            t->setComputationEndDate(compute_action->getEndDate());// could be -1.0

            /*
             * Look at the file write actions
             */

            // Create filewrite time stamps
            for (auto const &a: this->task_file_write_actions[t]) {
                auto fwa = std::dynamic_pointer_cast<FileWriteAction>(a);
                auto fwa_file = fwa->getFile();
                switch (a->getState()) {
                    case Action::NOT_READY:
                    case Action::READY:
                        break;
                    case Action::STARTED:
                        simulation->getOutput().addTimestampFileWriteStart(fwa->getStartDate(), fwa_file, fwa->getFileLocation(), fwa->getFileLocation()->getStorageService(), t);
                        break;
                    case Action::COMPLETED:
                        simulation->getOutput().addTimestampFileWriteStart(fwa->getStartDate(), fwa_file, fwa->getFileLocation(), fwa->getFileLocation()->getStorageService(), t);
                        simulation->getOutput().addTimestampFileWriteCompletion(fwa->getEndDate(), fwa_file, fwa->getFileLocation(), fwa->getFileLocation()->getStorageService(), t);
                        break;
                    case Action::KILLED:
                    case Action::FAILED:
                        simulation->getOutput().addTimestampFileWriteStart(fwa->getStartDate(), fwa_file, fwa->getFileLocation(), fwa->getFileLocation()->getStorageService(), t);
                        simulation->getOutput().addTimestampFileWriteFailure(fwa->getEndDate(), fwa_file, fwa->getFileLocation(), fwa->getFileLocation()->getStorageService(), t);
                        break;
                }
            }

            this->analyzeActions(this->task_file_write_actions[t],
                                 &at_least_one_failed,
                                 &at_least_one_killed,
                                 &failure_cause,
                                 &earliest_start_date,
                                 &latest_end_date,
                                 &earliest_failure_date);

            if (at_least_one_failed or at_least_one_killed) {
                t->setWriteOutputStartDate(earliest_start_date);
                state_changes[t] = WorkflowTask::State::READY;// This may be changed to NOT_READY later based on other tasks

                if (at_least_one_killed) {
                    job_failure_cause = std::make_shared<JobKilled>(this->getSharedPtr());
                    failure_count_increments.insert(t);
                    t->setFailureDate(earliest_failure_date);
                    simulation->getOutput().addTimestampTaskTermination(earliest_failure_date, t);
                } else if (at_least_one_failed) {
                    t->setFailureDate(earliest_failure_date);
                    simulation->getOutput().addTimestampTaskFailure(earliest_failure_date, t);
                    failure_count_increments.insert(t);
                    if (job_failure_cause == nullptr) job_failure_cause = failure_cause;
                }

                continue;
            }

            if (earliest_start_date == -1) earliest_start_date = compute_action->getEndDate();
            if (latest_end_date == -1) latest_end_date = compute_action->getEndDate();
            t->setWriteOutputStartDate(earliest_start_date);
            t->setWriteOutputEndDate(latest_end_date);
            state_changes[t] = WorkflowTask::State::COMPLETED;
            t->setEndDate(latest_end_date);
            simulation->getOutput().addTimestampTaskCompletion(latest_end_date, t);
        }

        /*
        * Look at Post-File copy actions
        */
        if (not this->post_file_copy_actions.empty()) {
            bool at_least_one_failed, at_least_one_killed;
            std::shared_ptr<FailureCause> failure_cause;
            double earliest_start_date, latest_end_date, earliest_failure_date;
            this->analyzeActions(this->post_file_copy_actions,
                                 &at_least_one_failed,
                                 &at_least_one_killed,
                                 &failure_cause,
                                 &earliest_start_date,
                                 &latest_end_date,
                                 &earliest_failure_date);
            if (at_least_one_killed) {
                job_failure_cause = std::make_shared<JobKilled>(this->getSharedPtr());
            } else if (at_least_one_failed) {
                job_failure_cause = failure_cause;
                return;
            }
        }

        /*
        * Look at Cleanup file_deletion actions
        */
        if (not this->cleanup_actions.empty()) {
            bool at_least_one_failed, at_least_one_killed;
            std::shared_ptr<FailureCause> failure_cause;
            double earliest_start_date, latest_end_date, earliest_failure_date;
            this->analyzeActions(this->cleanup_actions,
                                 &at_least_one_failed,
                                 &at_least_one_killed,
                                 &failure_cause,
                                 &earliest_start_date,
                                 &latest_end_date,
                                 &earliest_failure_date);
            if (at_least_one_killed) {
                job_failure_cause = std::make_shared<JobKilled>(this->getSharedPtr());
                return;
            } else if (at_least_one_failed) {
                job_failure_cause = failure_cause;
                return;
            }
        }

        /** Let's not care about the SCRATCH cleanup action. If it failed, oh well **/
    }

    /**
 * @brief Apply updates to tasks
 * @param state_changes: state changes
 * @param failure_count_increments: set of tasks whose failure counts should be incremented by one
 */
    void StandardJob::applyTaskUpdates(const map<std::shared_ptr<WorkflowTask>, WorkflowTask::State> &state_changes,
                                       const set<std::shared_ptr<WorkflowTask>> &failure_count_increments) {
        // Update task states
        for (auto &state_update: state_changes) {
            std::shared_ptr<WorkflowTask> task = state_update.first;
            task->setState(state_update.second);
        }

        // Update task readiness-es
        for (auto &state_update: state_changes) {
            std::shared_ptr<WorkflowTask> task = state_update.first;
            task->updateReadiness();
            if (task->getState() == WorkflowTask::State::COMPLETED) {
                for (auto const &child: task->getChildren()) {
                    child->updateReadiness();
                }
            }
        }

        // Update task failure counts if any
        for (const auto &task: failure_count_increments) {
            task->incrementFailureCount();
        }
    }

    /**
 * @brief Determines whether the job's spec uses scratch space
 * @return
 */
    bool StandardJob::usesScratch() {
        for (const auto &fl: this->file_locations) {
            for (auto const &fl_l: fl.second) {
                if (fl_l->isScratch()) {
                    return true;
                }
            }
        }

        for (auto const &task: this->tasks) {
            for (auto const &f: task->getInputFiles()) {
                if (this->file_locations.find(f) == this->file_locations.end()) {
                    return true;
                }
            }
            for (auto const &f: task->getOutputFiles()) {
                if (this->file_locations.find(f) == this->file_locations.end()) {
                    return true;
                }
            }
        }

        for (auto const &pfc: this->pre_file_copies) {
            if ((std::get<0>(pfc)->isScratch()) or (std::get<1>(pfc)->isScratch())) {
                return true;
            }
        }

        for (auto const &pfc: this->post_file_copies) {
            if ((std::get<0>(pfc)->isScratch()) or (std::get<1>(pfc)->isScratch())) {
                return true;
            }
        }
        return false;
    }

}// namespace wrench
