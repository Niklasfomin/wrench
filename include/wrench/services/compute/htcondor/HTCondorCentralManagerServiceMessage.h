/**
 * Copyright (c) 2017-2019. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_HTCONDORCENTRALMANAGERSERVICEMESSAGE_H
#define WRENCH_HTCONDORCENTRALMANAGERSERVICEMESSAGE_H

#include "wrench/services/ServiceMessage.h"
#include "wrench/job/StandardJob.h"
#include "wrench/job/Job.h"

#include <vector>

namespace wrench {

    /***********************/
    /** \cond INTERNAL     */
    /***********************/

    /**
     * @brief Top-level class for messages received/sent by a HTCondorCentralManagerService
     */
    class HTCondorCentralManagerServiceMessage : public ServiceMessage {
    protected:
        HTCondorCentralManagerServiceMessage(sg_size_t payload);
    };

    /**
     * @brief A message received by a HTCondorCentralManagerService so that it is notified of a negotiator
     *        cycle completion
     */
    class NegotiatorCompletionMessage : public HTCondorCentralManagerServiceMessage {
    public:
        NegotiatorCompletionMessage(std::set<std::shared_ptr<Job>> scheduled_jobs, sg_size_t payload);

        /** @brief List of scheduled jobs */
        std::set<std::shared_ptr<Job>> scheduled_jobs;
    };

    /**
     * @brief A message received by a HTCondorCentralManagerService so that it wakes up and
     * tries to dispatch jobs again
     */
    class CentralManagerWakeUpMessage : public HTCondorCentralManagerServiceMessage {
    public:
        explicit CentralManagerWakeUpMessage(sg_size_t payload);
    };

    /***********************/
    /** \endcond INTERNAL  */
    /***********************/
}// namespace wrench

#endif//WRENCH_HTCONDORCENTRALMANAGERSERVICEMESSAGE_H
