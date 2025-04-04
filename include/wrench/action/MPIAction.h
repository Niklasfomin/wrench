/**
* Copyright (c) 2017-2019. The WRENCH Team.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*/

#ifndef WRENCH_MPI_ACTION_H
#define WRENCH_MPI_ACTION_H

#include <string>
#include <utility>

#include "wrench/execution_controller/ExecutionController.h"
#include "wrench/action/Action.h"

namespace wrench {


    /***********************/
    /** \cond DEVELOPER    */
    /***********************/

    /**
     * @brief A class that implements a custom action
     */
    class MPIAction : public Action {

    public:
        unsigned long getNumProcesses() const;
        unsigned long getNumCoresPerProcess() const;

    protected:
        friend class CompoundJob;

        MPIAction(const std::string &name,
                  unsigned long num_processes,
                  unsigned long num_cores_per_process,
                  const std::function<void(const std::shared_ptr<ExecutionController> &controller)>& lambda_mpi);

        unsigned long getMinNumCores() const override;
        unsigned long getMaxNumCores() const override;
        sg_size_t getMinRAMFootprint() const override;

        void execute(const std::shared_ptr<ActionExecutor> &action_executor) override;
        void terminate(const std::shared_ptr<ActionExecutor> &action_executor) override;

    private:
        unsigned long num_processes;
        unsigned long num_cores_per_process;

        std::function<void(const std::shared_ptr<ExecutionController> &controller)> lambda_mpi;

        class MPIPrivateExecutionController : public ExecutionController {
        public:
            MPIPrivateExecutionController(const std::string &hostname, const std::string &suffix) : ExecutionController(hostname, suffix) {}

            int main() override {
                simgrid::s4u::this_actor::suspend();
                return 0;
            }
        };

        class MPIProcess {
        public:
            MPIProcess(std::function<void(const std::shared_ptr<ActionExecutor> &action_executor)> lambda_mpi, simgrid::s4u::BarrierPtr barrier,
                       std::shared_ptr<ActionExecutor> action_executor) : lambda_mpi(std::move(lambda_mpi)), barrier(barrier), action_executor(std::move(action_executor)) {}

            void operator()() {
                this->lambda_mpi(action_executor);
                barrier->wait();
            }

            /**
             * @brief Retrieve the executor in charge of this MPI action's execution
             * @return
             */
            std::shared_ptr<ActionExecutor> getActionExecutor() {
                return this->action_executor;
            }

        private:
            std::function<void(const std::shared_ptr<ActionExecutor> &action_executor)> lambda_mpi;
            simgrid::s4u::BarrierPtr barrier;
            std::shared_ptr<ActionExecutor> action_executor;
        };
    };


    /***********************/
    /** \endcond           */
    /***********************/

}// namespace wrench

#endif//WRENCH_MPI_ACTION_H
