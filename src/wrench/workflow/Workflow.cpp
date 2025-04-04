/**
 * Copyright (c) 2017-2021. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <wrench/workflow/WorkflowTask.h>
#include <wrench/simulation/Simulation.h>
#include <wrench/logging/TerminalOutput.h>
#include <wrench/workflow/Workflow.h>

WRENCH_LOG_CATEGORY(wrench_core_workflow, "Log category for Workflow");

namespace wrench {

    /**
     * @brief Method that will delete all workflow tasks and all files used
     *         by these tasks
     */
    void Workflow::clear() {
        this->tasks.clear();
        //        for (auto const &f: this->data_files) {
        //            //            std::cerr << "SIMULATION REMOVING FILE " << f->getID() << "\n";
        //            Simulation::removeFile(f);
        //        }
        //        this->data_files.clear();
    }

    /**
     * @brief Create and add a new computational task to the workflow
     *
     * @param id: a unique string id
     * @param flops: number of flops
     * @param min_num_cores: the minimum number of cores required to run the task
     * @param max_num_cores: the maximum number of cores that can be used by the task (use INT_MAX for infinity)
     * @param memory_requirement: memory_manager_service requirement (in bytes)
     *
     * @return the WorkflowTask instance
     *
     */
    std::shared_ptr<WorkflowTask> Workflow::addTask(const std::string &id,
                                                    double flops,
                                                    unsigned long min_num_cores,
                                                    unsigned long max_num_cores,
                                                    sg_size_t memory_requirement) {
        if ((flops < 0.0) || (min_num_cores < 1) || (min_num_cores > max_num_cores)) {
            throw std::invalid_argument("WorkflowTask::addTask(): Invalid argument");
        }

        // Check that the task doesn't really exist
        if (tasks.find(id) != tasks.end()) {
            throw std::invalid_argument("Workflow::addTask(): Task ID '" + id + "' already exists");
        }

        // Create the WorkflowTask object
        auto task = std::shared_ptr<WorkflowTask>(new WorkflowTask(id, flops, min_num_cores, max_num_cores,
                                                                   memory_requirement));
        this->ready_tasks.insert(task);
        // Associate the workflow to the task
        task->workflow = this;

        task->toplevel = 0;// upon creation, a task is an exit task

        // Create a DAG node for it
        this->dag.addVertex(task.get());

        tasks[task->id] = task;// owner

        return task;
    }

    /**
     * @brief Remove a file from the workflow (but not from the simulation)
     * @param file: a file
     *
     */
    void Workflow::removeFile(const std::shared_ptr<DataFile> &file) {
        if (this->task_output_files.find(file) != this->task_output_files.end()) {
            throw std::invalid_argument("Workflow::removeFile(): File " +
                                        file->getID() + " cannot be removed because it is output of task " +
                                        this->task_output_files[file]->getID());
        }

        if (this->task_input_files.find(file) != this->task_input_files.end() and
            (not this->task_input_files[file].empty())) {
            throw std::invalid_argument("Workflow::removeFile(): File " +
                                        file->getID() + " cannot be removed because it is input to " +
                                        std::to_string(this->task_input_files[file].size()) + " tasks");
        }

        this->task_output_files.erase(file);
        this->task_input_files.erase(file);
        this->data_files.erase(file->getID());
        //        Simulation::removeFile(file);
    }

    /**
     * @brief Remove a task from the workflow.
     *
     * @param task: a task
     *
     */
    void Workflow::removeTask(const std::shared_ptr<WorkflowTask> &task) {
        if (task == nullptr) {
            throw std::invalid_argument("Workflow::removeTask(): Invalid arguments");
        }

        // check that task exists (this should never happen)
        if (tasks.find(task->id) == tasks.end()) {
            throw std::invalid_argument("Workflow::removeTask(): Task '" + task->id + "' does not exist");
        }

        // Remove the task from the ready tasks, just in case
        if (this->ready_tasks.find(task) != this->ready_tasks.end()) {
            this->ready_tasks.erase(task);
        }

        // Fix all files
        for (auto &f: task->getInputFiles()) {
            this->task_input_files[f].erase(task);
            if (this->task_input_files[f].empty()) {
                this->task_input_files.erase(f);
            }
        }
        for (auto &f: task->getOutputFiles()) {
            this->task_output_files.erase(f);
        }


        // Get the children
        auto children = this->dag.getChildren(task.get());

        // Get the parents
        auto parents = this->dag.getParents(task.get());

        // Remove the task from the DAG
        this->dag.removeVertex(task.get());

        // Remove the task from the master list
        tasks.erase(tasks.find(task->id));

        // Make the children ready, if the case
        for (auto const &child: children) {
            Workflow::updateReadiness(child);
        }

        // Brute-force update of the top-level of all the children and the bottom-level
        // of the parents of the removed task (if we're doing it dynamically)
        if (this->update_top_bottom_levels_dynamically) {
            for (auto const &child: children) {
                child->updateTopLevel();
            }
            for (auto const &parent: parents) {
                parent->updateBottomLevel();
            }
        }
    }

    /**
     * @brief Find a WorkflowTask based on its ID
     *
     * @param id: a string id
     *
     * @return a workflow task (or throws a std::invalid_argument if not found)
     *
     */
    std::shared_ptr<WorkflowTask> Workflow::getTaskByID(const std::string &id) {
        if (tasks.find(id) == tasks.end()) {
            throw std::invalid_argument("Workflow::getTaskByID(): Unknown WorkflowTask ID " + id);
        }
        return tasks[id];
    }

    /**
     * @brief Create a control dependency between two workflow tasks. Will not
     *        do anything if there is already a path between the two tasks.
     *
     * @param src: the parent task
     * @param dst: the child task
     * @param redundant_dependencies: whether DAG redundant dependencies should be kept in the graph
     *
     */
    void Workflow::addControlDependency(const std::shared_ptr<WorkflowTask> &src, const std::shared_ptr<WorkflowTask> &dst, bool redundant_dependencies) {
        if ((src == nullptr) || (dst == nullptr)) {
            throw std::invalid_argument("Workflow::addControlDependency(): Invalid arguments");
        }

        if (src == dst) {
            return;
        }

        if (this->dag.doesPathExist(dst.get(), src.get())) {
            throw std::runtime_error("Workflow::addControlDependency(): Adding dependency between task " + src->getID() +
                                     " and " + dst->getID() + " would create a cycle in the workflow graph");
        }

        if (redundant_dependencies || not this->dag.doesPathExist(src.get(), dst.get())) {
            WRENCH_DEBUG("Adding control dependency %s-->%s", src->getID().c_str(), dst->getID().c_str());
            this->dag.addEdge(src.get(), dst.get());

            if (this->update_top_bottom_levels_dynamically) {
                dst->updateTopLevel();
                src->updateBottomLevel();
            }

            if (src->getState() != WorkflowTask::State::COMPLETED) {
                dst->setInternalState(WorkflowTask::InternalState::TASK_NOT_READY);
                dst->setState(WorkflowTask::State::NOT_READY);
            }
        }
    }

    /**
     * @brief Remove a control dependency between tasks  (does nothing if none)
     * @param src: the source task
     * @param dst: the destination task
     */
    void Workflow::removeControlDependency(const std::shared_ptr<WorkflowTask> &src, const std::shared_ptr<WorkflowTask> &dst) {
        if ((src == nullptr) || (dst == nullptr)) {
            throw std::invalid_argument("Workflow::removeControlDependency(): Invalid arguments");
        }

        /*  Check that the two tasks don't have a data dependency; if so, just return */
        for (auto const &f: dst->getInputFiles()) {
            if (this->task_output_files[f] == src) {
                return;
            }
        }

        /* If there is an edge between the two tasks, remove it */
        if (this->dag.doesEdgeExist(src.get(), dst.get())) {
            this->dag.removeEdge(src.get(), dst.get());

            if (this->update_top_bottom_levels_dynamically) {
                dst->updateTopLevel();
                src->updateBottomLevel();
            }

            /* Update state */
            Workflow::updateReadiness(dst.get());
        }
    }

    /**
     * @brief Update the readiness of a task
     * @param task : workflow task
     */
    void Workflow::updateReadiness(WorkflowTask *task) {
        if ((task->getState() == WorkflowTask::State::NOT_READY) and (task->getInternalState() == WorkflowTask::InternalState::TASK_NOT_READY)) {
            bool ready = true;
            for (auto const &p: task->getParents()) {
                if (p->getState() != WorkflowTask::State::COMPLETED) {
                    ready = false;
                    break;
                }
            }
            if (ready) {
                task->setInternalState(WorkflowTask::InternalState::TASK_READY);
                task->setState(WorkflowTask::State::READY);
            }
        }
    }

    /**
     * @brief Get the number of tasks in the workflow
     *
     * @return the number of tasks
     */
    unsigned long Workflow::getNumberOfTasks() const {
        return this->tasks.size();
    }

    /**
     * @brief Determine whether one source is an ancestor of a destination task
     *
     * @param src: the source task
     * @param dst: the destination task
     *
     * @return true if there is a path from src to dst, false otherwise
     */
    bool Workflow::pathExists(const std::shared_ptr<WorkflowTask> &src, const std::shared_ptr<WorkflowTask> &dst) {
        return this->dag.doesPathExist(src.get(), dst.get());
    }

    /**
     * @brief  Constructor
     */
    Workflow::Workflow() {
        static int workflow_number = 0;
        this->update_top_bottom_levels_dynamically = true;
        this->name = "workflow_" + std::to_string(workflow_number++);
    }

    /**
     * @brief Name getter
     * @return The workflow's name
     */
    std::string Workflow::getName() const {
        return this->name;
    }

    /**
     * @brief Get a vector of ready tasks
     *
     * @return a vector of tasks
     */
    std::vector<std::shared_ptr<WorkflowTask>> Workflow::getReadyTasks() {
        std::vector<std::shared_ptr<WorkflowTask>> vector_of_ready_tasks(this->ready_tasks.begin(), this->ready_tasks.end());
        return vector_of_ready_tasks;
    }

    /**
     * @brief Get a map of clusters composed of ready tasks
     *
     * @return map of workflow cluster tasks
     */
    std::map<std::string, std::vector<std::shared_ptr<WorkflowTask>>> Workflow::getReadyClusters() const {
        // TODO: Implement this more efficiently

        std::map<std::string, std::vector<std::shared_ptr<WorkflowTask>>> task_map;

        for (auto &it: this->tasks) {
            std::shared_ptr<WorkflowTask> task = it.second;

            if (task->getState() == WorkflowTask::State::READY) {
                if (task->getClusterID().empty()) {
                    task_map[task->getID()] = {task};

                } else {
                    if (task_map.find(task->getClusterID()) == task_map.end()) {
                        task_map[task->getClusterID()] = {task};
                    } else {
                        // add to clustered task
                        task_map[task->getClusterID()].push_back(task);
                    }
                }
            } else {
                if (task_map.find(task->getClusterID()) != task_map.end()) {
                    task_map[task->getClusterID()].push_back(task);
                }
            }
        }
        return task_map;
    }

    /**
     * @brief Returns whether all tasks are complete
     *
     * @return true or false
     */
    bool Workflow::isDone() const {
        for (const auto &it: this->tasks) {
            auto task = it.second;
            if (task->getState() != WorkflowTask::State::COMPLETED) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Get the list of all tasks in the workflow
     *
     * @return a copy of themap of tasks, indexed by ID
     */
    std::map<std::string, std::shared_ptr<WorkflowTask>> Workflow::getTaskMap() {
        return this->tasks;
    }

    /**
     * @brief Get the list of all tasks in the workflow
     *
     * @return a vector of tasks
     */
    std::vector<std::shared_ptr<WorkflowTask>> Workflow::getTasks() const {
        std::vector<std::shared_ptr<WorkflowTask>> all_tasks;
        for (auto const &t: this->tasks) {
            all_tasks.push_back(t.second);
        }
        return all_tasks;
    }

    /**
     * @brief Get the list of children for a task
     *
     * @param task: a workflow task
     *
     * @return a vector of tasks
     */
    std::vector<std::shared_ptr<WorkflowTask>> Workflow::getTaskChildren(const std::shared_ptr<WorkflowTask> &task) {
        if (task == nullptr) {
            throw std::invalid_argument("Workflow::getTaskChildren(): Invalid arguments");
        }
        auto raw_ptrs = this->dag.getChildren(task.get());
        std::vector<std::shared_ptr<WorkflowTask>> shared_ptrs;
        shared_ptrs.reserve(raw_ptrs.size());
        for (const auto &raw_ptr: raw_ptrs) {
            shared_ptrs.push_back(raw_ptr->getSharedPtr());
        }

        return shared_ptrs;
    }

    /**
     * @brief Get the number of children for a task
     *
     * @param task: a workflow task
     *
     * @return a number of children
     */
    long Workflow::getTaskNumberOfChildren(const std::shared_ptr<WorkflowTask> &task) {
        if (task == nullptr) {
            throw std::invalid_argument("Workflow::getTaskNumberOfChildren(): Invalid arguments");
        }
        return this->dag.getNumberOfChildren(task.get());
    }

    /**
     * @brief Get the list of parents for a task
     *
     * @param task: a workflow task
     *
     * @return a vector of tasks
     */
    std::vector<std::shared_ptr<WorkflowTask>> Workflow::getTaskParents(const std::shared_ptr<WorkflowTask> &task) {
        if (task == nullptr) {
            throw std::invalid_argument("Workflow::getTaskParents(): Invalid arguments");
        }
        auto raw_ptrs = this->dag.getParents(task.get());
        std::vector<std::shared_ptr<WorkflowTask>> shared_ptrs;
        shared_ptrs.reserve(raw_ptrs.size());
        for (auto const &raw_ptr: raw_ptrs) {
            shared_ptrs.push_back(raw_ptr->getSharedPtr());
        }
        return shared_ptrs;
    }

    /**
     * @brief Get the number of parents for a task
     *
     * @param task: a workflow task
     *
     * @return a number of parents
     */
    long Workflow::getTaskNumberOfParents(const std::shared_ptr<WorkflowTask> &task) {
        if (task == nullptr) {
            throw std::invalid_argument("Workflow::getTaskNumberOfParents(): Invalid arguments");
        }
        return this->dag.getNumberOfParents(task.get());
    }

    /**
     * @brief Retrieve the list of the input files of the workflow (i.e., those files
     *        that are input to some tasks but output from none)
     *
     * @return a map of files indexed by file ID
     */
    std::map<std::string, std::shared_ptr<DataFile>> Workflow::getInputFileMap() const {
        std::map<std::string, std::shared_ptr<DataFile>> input_files;
        for (auto const &f: Simulation::getFileMap()) {
            // If the file is not input to any task, then it can't be what we want
            if (this->task_input_files.find(f.second) == this->task_input_files.end()) {
                continue;
            }
            // If the file is output to a task, then it can't be what we want
            if (this->task_output_files.find(f.second) != this->task_output_files.end()) {
                continue;
            }
            input_files[f.second->getID()] = f.second;
        }
        return input_files;
    }

    /**
     * @brief Retrieve the list of the input files of the workflow (i.e., those files
     *        that are input to some tasks but output from none)
     *
     * @return a vector of files
     */
    std::vector<std::shared_ptr<DataFile>> Workflow::getInputFiles() const {
        std::vector<std::shared_ptr<DataFile>> input_files;
        for (auto const &f: Simulation::getFileMap()) {
            // If the file is not input to any task, then it can't be what we want
            if (this->task_input_files.find(f.second) == this->task_input_files.end()) {
                continue;
            }
            // If the file is output to a task, then it can't be what we want
            if (this->task_output_files.find(f.second) != this->task_output_files.end()) {
                continue;
            }
            input_files.push_back(f.second);
        }
        return input_files;
    }

    /**
    * @brief Retrieve a list of the output files of the workflow (i.e., those files
    *        that are output from some tasks but input to none)
    *
    * @return a map of files indexed by ID
    */
    std::map<std::string, std::shared_ptr<DataFile>> Workflow::getOutputFileMap() const {
        std::map<std::string, std::shared_ptr<DataFile>> output_files;
        for (auto const &f: Simulation::getFileMap()) {
            // If the file is not output to any task, then it can't be what we want
            if (this->task_output_files.find(f.second) == this->task_output_files.end()) {
                continue;
            }
            // If the file is input to a task, then it can't be what we want
            if (this->task_input_files.find(f.second) != this->task_input_files.end() and (not this->task_input_files.at(f.second).empty())) {
                continue;
            }
            output_files[f.second->getID()] = f.second;
        }
        return output_files;
    }

    /**
   * @brief Retrieve a list of the output files of the workflow (i.e., those files
   *        that are output from some tasks but input to none)
   *
   * @return a vector of files
   */
    std::vector<std::shared_ptr<DataFile>> Workflow::getOutputFiles() const {
        std::vector<std::shared_ptr<DataFile>> output_files;
        for (auto const &f: Simulation::getFileMap()) {
            // If the file is not output to any task, then it can't be what we want
            if (this->task_output_files.find(f.second) == this->task_output_files.end()) {
                continue;
            }
            // If the file is input to a task, then it can't be what we want
            if (this->task_input_files.find(f.second) != this->task_input_files.end() and (not this->task_input_files.at(f.second).empty())) {
                continue;
            }
            output_files.push_back(f.second);
        }
        return output_files;
    }

    /**
     * @brief Get the total number of flops for a list of tasks
     *
     * @param tasks: a vector of tasks
     *
     * @return the total number of flops
     */
    double Workflow::getSumFlops(const std::vector<std::shared_ptr<WorkflowTask>> &tasks) {
        double total_flops = 0;
        for (auto const &task: tasks) {
            total_flops += task->getFlops();
        }
        return total_flops;
    }

    /**
     * @brief Returns all tasks with top-levels in a range
     * @param min: the low end of the range (inclusive)
     * @param max: the high end of the range (inclusive)
     * @return a vector of tasks
     */
    std::vector<std::shared_ptr<WorkflowTask>> Workflow::getTasksInTopLevelRange(int min, int max) const {
        std::vector<std::shared_ptr<WorkflowTask>> to_return;
        for (auto const &task: this->getTasks()) {
            if ((task->getTopLevel() >= min) and (task->getTopLevel() <= max)) {
                to_return.push_back(task);
            }
        }
        return to_return;
    }

    /**
     * @brief Returns all tasks with bottom-levels in a range
     * @param min: the low end of the range (inclusive)
     * @param max: the high end of the range (inclusive)
     * @return a vector of tasks
     */
    std::vector<std::shared_ptr<WorkflowTask>> Workflow::getTasksInBottomLevelRange(int min, int max) const {
        std::vector<std::shared_ptr<WorkflowTask>> to_return;
        for (auto const &task: this->getTasks()) {
            if ((task->getBottomLevel() >= min) and (task->getBottomLevel() <= max)) {
                to_return.push_back(task);
            }
        }
        return to_return;
    }

    /**
     * @brief Get the list of exit tasks of the workflow, i.e., those tasks
     *        that don't have parents
     * @return A map of tasks indexed by their IDs
     */
    std::map<std::string, std::shared_ptr<WorkflowTask>> Workflow::getEntryTaskMap() const {
        // TODO: This could be done more efficiently at the DAG level
        std::map<std::string, std::shared_ptr<WorkflowTask>> entry_tasks;
        for (auto const &t: this->tasks) {
            auto task = t.second;
            if (task->getNumberOfParents() == 0) {
                entry_tasks[task->getID()] = task;
            }
        }
        return entry_tasks;
    }

    /**
     * @brief Get the list of exit tasks of the workflow, i.e., those tasks
     *        that don't have parents
     * @return A vector of tasks
     */
    std::vector<std::shared_ptr<WorkflowTask>> Workflow::getEntryTasks() const {
        // TODO: This could be done more efficiently at the DAG level
        std::vector<std::shared_ptr<WorkflowTask>> entry_tasks;
        for (auto const &t: this->tasks) {
            auto task = t.second;
            if (task->getNumberOfParents() == 0) {
                entry_tasks.push_back(task);
            }
        }
        return entry_tasks;
    }

    /**
     * @brief Get the exit tasks of the workflow, i.e., those tasks
     *        that don't have children
     * @return A map of tasks indexed by their IDs
     */
    std::map<std::string, std::shared_ptr<WorkflowTask>> Workflow::getExitTaskMap() const {
        // TODO: This could be done more efficiently at the DAG level
        std::map<std::string, std::shared_ptr<WorkflowTask>> exit_tasks;
        for (auto const &t: this->tasks) {
            auto task = t.second;
            if (task->getNumberOfChildren() == 0) {
                exit_tasks[task->getID()] = task;
            }
        }
        return exit_tasks;
    }

    /**
    * @brief Get the exit tasks of the workflow, i.e., those tasks
    *        that don't have children
    * @return A vector of tasks
    */
    std::vector<std::shared_ptr<WorkflowTask>> Workflow::getExitTasks() const {
        // TODO: This could be done more efficiently at the DAG level
        std::vector<std::shared_ptr<WorkflowTask>> exit_tasks;
        for (auto const &t: this->tasks) {
            auto task = t.second;
            if (task->getNumberOfChildren() == 0) {
                exit_tasks.push_back(task);
            }
        }
        return exit_tasks;
    }

    /**
     * @brief Returns the number of levels in the workflow
     * @return the number of levels
     */
    unsigned long Workflow::getNumLevels() const {
        int max_top_level = 0;
        for (auto const &t: this->tasks) {
            auto task = t.second.get();
            if (task->getNumberOfChildren() == 0) {
                if (1 + task->getTopLevel() > max_top_level) {
                    max_top_level = 1 + task->getTopLevel();
                }
            }
        }
        return max_top_level;
    }

    /**
     * @brief Returns the workflow's completion date
     * @return a date in seconds (or a negative value
     *        If the workflow has not completed)
     */
    double Workflow::getCompletionDate() const {
        double completion_date = -1.0;
        for (auto const &task: this->tasks) {
            if (task.second->getState() != WorkflowTask::State::COMPLETED) {
                completion_date = -1.0;
                break;
            } else {
                completion_date = std::max<double>(completion_date, task.second->getEndDate());
            }
        }
        return completion_date;
    }

    /**
     * @brief Returns the workflow's start date
     * @return a date in seconds (or a negative value
     *        if no workflow task has successfully completed)
     */
    double Workflow::getStartDate() const {
        double start_date = -1.0;
        for (auto const &task: this->tasks) {
            if (task.second->getState() == WorkflowTask::State::COMPLETED) {
                start_date = std::min<double>(start_date, task.second->getStartDate());
            }
        }
        return start_date;
    }


    /**
     * @brief Get the workflow task for which a file is an output
     * @param file: a file
     * @return at task (or nullptr)
     */
    std::shared_ptr<WorkflowTask> Workflow::getTaskThatOutputs(const std::shared_ptr<DataFile> &file) {
        if (this->task_output_files.find(file) == this->task_output_files.end()) {
            return nullptr;
        } else {
            return this->task_output_files[file];
        }
    }

    /**
     * @brief Determine whether a file is output of some task
     * @param file: a file
     * @return true or false
     */
    bool Workflow::isFileOutputOfSomeTask(const std::shared_ptr<DataFile> &file) {
        return (this->task_output_files.find(file) != this->task_output_files.end());
    }

    /**
     * @brief Find which tasks use a file as input
     * @param file : a file
     * @return a vector of tasks
     */
    std::set<std::shared_ptr<WorkflowTask>> Workflow::getTasksThatInput(const std::shared_ptr<DataFile> &file) {
        std::set<std::shared_ptr<WorkflowTask>> to_return;
        if (this->task_input_files.find(file) != this->task_input_files.end()) {
            to_return = this->task_input_files[file];
        }
        return to_return;
    }

    /**
      * @brief Get the list of all files in the workflow/simulation
      *
      * @return a reference to the map of files in the workflow/simulation, indexed by file ID
      */
    std::map<std::string, std::shared_ptr<DataFile>> &Workflow::getFileMap() {
        return Simulation::getFileMap();
    }

    //    /**
    //     * @brief Get a file based on its ID
    //     * @param id : file ID
    //     * @return a file
    //     */
    //    std::shared_ptr<DataFile> Workflow::getFileByID(const std::string &id) {
    //        return Simulation::getFileByID(id);
    //    }

    /**
     * @brief Create a workflow instance
     * @return
     */
    std::shared_ptr<Workflow> Workflow::createWorkflow() {
        return std::shared_ptr<Workflow>(new Workflow());
    }

    /**
    * @brief Enable dynamic top/bottom level updates
    * @param enabled: true if dynamic updates are to be enabled, false otherwise
    */
    void Workflow::enableTopBottomLevelDynamicUpdates(bool enabled) {
        this->update_top_bottom_levels_dynamically = enabled;
    }

    /**
     * @brief Update the top level of all tasks (in case dynamic top level updates
     * had been disabled)
     */
    void Workflow::updateAllTopBottomLevels() {

        // Compute entry tasks
        std::vector<std::shared_ptr<WorkflowTask>> entry_tasks;
        for (auto const &t: this->tasks) {
            if (t.second->getNumberOfParents() == 0) {
                entry_tasks.push_back(t.second);
            }
        }
        // Compute exit tasks
        std::vector<std::shared_ptr<WorkflowTask>> exit_tasks;
        for (auto const &t: this->tasks) {
            if (t.second->getNumberOfChildren() == 0) {
                exit_tasks.push_back(t.second);
            }
        }

        // Reset all levels to -1 for memoization purposes
        for (auto const &t: this->tasks) {
            t.second->toplevel = -1;
            t.second->bottomlevel = -1;
        }

        // Update top levels recursively
        for (auto const &et: exit_tasks) {
            et->computeTopLevel();
        }

        // Update bottom levels recursively
        for (auto const &et: entry_tasks) {
            et->computeBottomLevel();
        }
    }

}// namespace wrench
