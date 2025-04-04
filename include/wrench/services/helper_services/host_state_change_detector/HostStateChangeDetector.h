/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_HOSTSTATECHANGEDETECTOR_H
#define WRENCH_HOSTSTATECHANGEDETECTOR_H

#include "wrench/services/Service.h"
#include "wrench/simgrid_S4U_util/S4U_Daemon.h"
#include "HostStateChangeDetectorProperty.h"

namespace wrench {

    /***********************/
    /** \cond INTERNAL     */
    /***********************/

    /**
     * @brief A service that detects and reports on host state changes (turned on, turned off)
     */
    class HostStateChangeDetector : public Service {

    private:
        WRENCH_PROPERTY_COLLECTION_TYPE default_property_values = {};

    public:
        explicit HostStateChangeDetector(const std::string& host_on_which_to_run,
                                         const std::vector<simgrid::s4u::Host *>& hosts_to_monitor,
                                         bool notify_when_turned_on,
                                         bool notify_when_turned_off,
                                         bool notify_when_speed_change,
                                         const std::shared_ptr<S4U_Daemon>& creator,
                                         S4U_CommPort *commport_to_notify,
                                         WRENCH_PROPERTY_COLLECTION_TYPE property_list = {});

        void kill();


    private:
        void cleanup(bool has_returned_from_main, int return_value) override;
        void hostStateChangeCallback(const simgrid::s4u::Host *host);
        void hostSpeedChangeCallback(const simgrid::s4u::Host *host);

        std::vector<simgrid::s4u::Host *> hosts_to_monitor;
        bool notify_when_turned_on;
        bool notify_when_turned_off;
        bool notify_when_speed_change;
        S4U_CommPort *commport_to_notify;
        int main() override;

        std::vector<std::pair<std::string, bool>> hosts_that_have_recently_changed_state;
        std::vector<std::pair<std::string, double>> hosts_that_have_recently_changed_speed;

        std::shared_ptr<S4U_Daemon> creator;

        std::vector<std::string> hosts_that_have_recently_turned_on;
        std::vector<std::string> hosts_that_have_recently_turned_off;

        unsigned int on_state_change_call_back_id;
        unsigned int on_speed_change_call_back_id;
    };

    /***********************/
    /** \endcond           */
    /***********************/

}// namespace wrench


#endif//WRENCH_HOSTSTATECHANGEDETECTOR_H
