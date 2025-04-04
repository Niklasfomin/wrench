/**
 * Copyright (c) 2017-2019. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_WORKFLOW_H
#define WRENCH_WORKFLOW_H

#include <map>
#include <set>

#include "wrench/execution_events/ExecutionEvent.h"
#include "wrench/data_file/DataFile.h"
#include "WorkflowTask.h"
#include "DagOfTasks.h"

#include <boost/graph/adjacency_list.hpp>
#include "wrench/workflow/parallel_model/ParallelModel.h"

class WorkflowTask;

namespace wrench {

    class Simulation;


    /**
     * @brief A workflow (to be executed by a WMS)
     */
    class Workflow : public std::enable_shared_from_this<Workflow> {

    public:
        static std::shared_ptr<Workflow> createWorkflow();
        std::string getName() const;
        void clear();

        /**
         * @brief Get the shared pointer for this object
         * @return a shared pointer to the object
         */
        std::shared_ptr<Workflow> getSharedPtr() { return this->shared_from_this(); }


        std::shared_ptr<WorkflowTask> addTask(const std::string &, double flops,
                                              unsigned long min_num_cores,
                                              unsigned long max_num_cores,
                                              sg_size_t memory_requirement);

        void removeTask(const std::shared_ptr<WorkflowTask> &task);

        void removeFile(const std::shared_ptr<DataFile> &file);
        std::map<std::string, std::shared_ptr<DataFile>> &getFileMap();
        //        std::shared_ptr<DataFile> addFile(const std::string &id, double size);
        //        std::shared_ptr<DataFile> getFileByID(const std::string &id);
        std::shared_ptr<WorkflowTask> getTaskByID(const std::string &id);


        static double getSumFlops(const std::vector<std::shared_ptr<WorkflowTask>> &tasks);

        void addControlDependency(const std::shared_ptr<WorkflowTask> &src, const std::shared_ptr<WorkflowTask> &dst, bool redundant_dependencies = false);
        void removeControlDependency(const std::shared_ptr<WorkflowTask> &src, const std::shared_ptr<WorkflowTask> &dst);

        static void updateReadiness(WorkflowTask *task);

        unsigned long getNumberOfTasks() const;

        unsigned long getNumLevels() const;

        double getStartDate() const;
        double getCompletionDate() const;

        std::vector<std::shared_ptr<DataFile>> getInputFiles() const;
        std::map<std::string, std::shared_ptr<DataFile>> getInputFileMap() const;
        std::vector<std::shared_ptr<DataFile>> getOutputFiles() const;
        std::map<std::string, std::shared_ptr<DataFile>> getOutputFileMap() const;

        std::vector<std::shared_ptr<WorkflowTask>> getTasks() const;
        std::map<std::string, std::shared_ptr<WorkflowTask>> getTaskMap();
        std::map<std::string, std::shared_ptr<WorkflowTask>> getEntryTaskMap() const;
        std::vector<std::shared_ptr<WorkflowTask>> getEntryTasks() const;
        std::map<std::string, std::shared_ptr<WorkflowTask>> getExitTaskMap() const;
        std::vector<std::shared_ptr<WorkflowTask>> getExitTasks() const;

        std::vector<std::shared_ptr<WorkflowTask>> getTaskParents(const std::shared_ptr<WorkflowTask> &task);
        long getTaskNumberOfParents(const std::shared_ptr<WorkflowTask> &task);
        std::vector<std::shared_ptr<WorkflowTask>> getTaskChildren(const std::shared_ptr<WorkflowTask> &task);
        long getTaskNumberOfChildren(const std::shared_ptr<WorkflowTask> &task);

        bool pathExists(const std::shared_ptr<WorkflowTask> &src, const std::shared_ptr<WorkflowTask> &dst);

        std::shared_ptr<WorkflowTask> getTaskThatOutputs(const std::shared_ptr<DataFile> &file);
        bool isFileOutputOfSomeTask(const std::shared_ptr<DataFile> &file);

        std::set<std::shared_ptr<WorkflowTask>> getTasksThatInput(const std::shared_ptr<DataFile> &file);
        bool isDone() const;

        void enableTopBottomLevelDynamicUpdates(bool enabled);
        void updateAllTopBottomLevels();

        /***********************/
        /** \cond DEVELOPER    */
        /***********************/

        std::vector<std::shared_ptr<WorkflowTask>> getTasksInTopLevelRange(int min, int max) const;
        std::vector<std::shared_ptr<WorkflowTask>> getTasksInBottomLevelRange(int min, int max) const;

        std::vector<std::shared_ptr<WorkflowTask>> getReadyTasks();

        std::map<std::string, std::vector<std::shared_ptr<WorkflowTask>>> getReadyClusters() const;

        /***********************/
        /** \endcond           */
        /***********************/

    private:
        friend class WMS;
        friend class Simulation;
        friend class WorkflowTask;

        std::string name;
        bool update_top_bottom_levels_dynamically;

        Workflow();

        struct Vertex {
            std::shared_ptr<WorkflowTask> task;
        };
        typedef boost::adjacency_list<boost::listS, boost::vecS, boost::directedS, Vertex> DAG;
        typedef boost::graph_traits<DAG>::vertex_descriptor vertex_t;

        DagOfTasks dag;

        /* Map to find tasks by name */
        std::map<std::string, std::shared_ptr<WorkflowTask>> tasks;

        /* Set of ready tasks */
        std::set<std::shared_ptr<WorkflowTask>> ready_tasks;

        /* Map of output files */
        std::map<std::shared_ptr<DataFile>, std::shared_ptr<WorkflowTask>> task_output_files;
        std::map<std::shared_ptr<DataFile>, std::set<std::shared_ptr<WorkflowTask>>> task_input_files;

        /* files used in this workflow */
        std::map<std::string, std::shared_ptr<DataFile>> data_files;
    };
}// namespace wrench

#endif//WRENCH_WORKFLOW_H
