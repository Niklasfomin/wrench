/**
 * Copyright (c) 2017-2018. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef WRENCH_S4U_VIRTUALMACHINE_H
#define WRENCH_S4U_VIRTUALMACHINE_H

#include <simgrid/s4u/VirtualMachine.hpp>
#include <set>
#include <wrench/services/Service.h>


namespace wrench {

    /***********************/
    /** \cond INTERNAL     */
    /***********************/

    /**
     * @brief A wrapper for the simgrid::s4u::VirtualMachine class
     */
    class S4U_VirtualMachine {

    public:
        /**
         * @brief A map to keep track of VM-to-PM mapping
         */
        static std::unordered_map<std::string, std::string> vm_to_pm_map;

        /** @brief VM state enum */
        enum State {
            DOWN,
            RUNNING,
            SUSPENDED
        };

        S4U_VirtualMachine(const std::string &vm_name,
                           unsigned long num_cores,
                           sg_size_t ram_memory,
                           const WRENCH_PROPERTY_COLLECTION_TYPE& property_list,
                           const WRENCH_MESSAGE_PAYLOAD_COLLECTION_TYPE& messagepayload_list);

        void start(const std::string &pm_name);

        void suspend();

        void resume();

        void shutdown();

        void migrate(const std::string &dst_pm_name);

        std::string getPhysicalHostname();
        unsigned long getNumCores() const;
        sg_size_t getMemory() const;
        WRENCH_PROPERTY_COLLECTION_TYPE getPropertyList();
        WRENCH_MESSAGE_PAYLOAD_COLLECTION_TYPE getMessagePayloadList();

        State getState() const;
        std::string getStateAsString() const;


    private:
        State state;
        std::string vm_name;
        simgrid::s4u::VirtualMachine *vm = nullptr;
        unsigned long num_cores;
        sg_size_t ram_memory;
        std::string pm_name;
        WRENCH_PROPERTY_COLLECTION_TYPE property_list;
        WRENCH_MESSAGE_PAYLOAD_COLLECTION_TYPE messagepayload_list;
    };

    /***********************/
    /** \endcond           */
    /***********************/
}// namespace wrench

#endif//WRENCH_S4U_VIRTUALMACHINE_H
