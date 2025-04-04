/**
 * Copyright (c) 2017-2018. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_SIM4U_DAEMON_H
#define WRENCH_SIM4U_DAEMON_H

#include <string>

#include <simgrid/s4u.hpp>
#include <iostream>

//#include <wrench/simulation/Simulation.h>

//#define ACTOR_TRACKING_OUTPUT yes


namespace wrench {

    /***********************/
    /** \cond INTERNAL     */
    /***********************/

    class S4U_CommPort;
    class Simulation;

    /**
     * @brief A generic "running daemon" abstraction that serves as a basis for all simulated processes
     */
    class S4U_Daemon : public std::enable_shared_from_this<S4U_Daemon> {

        class LifeSaver {
        public:
            explicit LifeSaver(std::shared_ptr<S4U_Daemon> &reference) : reference(reference) {}

            std::shared_ptr<S4U_Daemon> reference;
        };


    public:
        /**
         * @brief A convenient map between actors and their default receive commports
         */
        static std::unordered_map<aid_t, S4U_CommPort *> map_actor_to_recv_commport;

        /**
         * @brief A map of actors to sets of held mutexes
         */
        static std::unordered_map<aid_t, std::set<simgrid::s4u::MutexPtr>> map_actor_to_held_mutexes;


        /** @brief The name of the daemon */
        std::string process_name;

        /** @brief The daemon's commport_name **/
        S4U_CommPort *commport;
        /** @brief The daemon's receive commport_name (to send to another daemon so that that daemon can reply) **/
        S4U_CommPort *recv_commport;

        /** @brief The name of the host on which the daemon is running */
        std::string hostname;

        static S4U_CommPort *getRunningActorRecvCommPort();

        S4U_Daemon(const std::string &hostname, const std::string &process_name_prefix);

        // Daemon without a commport_name (not needed?)
        //        S4U_Daemon(std::string hostname, std::string process_name_prefix);

        virtual ~S4U_Daemon();

        void startDaemon(bool daemonized, bool auto_restart);

        void createLifeSaver(std::shared_ptr<S4U_Daemon> reference);
        void deleteLifeSaver();

        virtual void cleanup(bool has_returned_from_main, int return_value);

        /**
         * @brief The daemon's main method, to be overridden
         * @return 0 on success, non-0 on failure!
         */
        virtual int main() = 0;

        bool hasReturnedFromMain() const;
        int getReturnValue() const;
        bool isDaemonized() const;
        bool isSetToAutoRestart() const;
        void setupOnExitFunction();

        std::pair<bool, int> join() const;

        void suspendActor() const;

        void resumeActor() const;


        std::string getName() const;

        /** @brief Daemon states */
        enum State {
            /** @brief CREATED state: the daemon has been created but not started */
            CREATED,
            /** @brief UP state: the daemon has been started and is still running */
            UP,
            /** @brief DOWN state: the daemon has been shutdown and/or has terminated */
            DOWN,
            /** @brief SUSPENDED state: the daemon has been suspended (and hopefully will be resumed0 */
            SUSPENDED,
        };

        S4U_Daemon::State getState() const;

        /** @brief The daemon's life saver */
        LifeSaver *life_saver = nullptr;

        /** @brief Method to acquire the daemon's lock */
        void acquireDaemonLock() const;

        /** @brief Method to release the daemon's lock */
        void releaseDaemonLock() const;

        Simulation *getSimulation() const;

        void setSimulation(Simulation *simulation);

        bool killActor();

    protected:
        /** @brief a pointer to the simulation object */
        Simulation *simulation_;

        /** @brief The service's state */
        State state;

        friend class S4U_DaemonActor;
        void runMainMethod();

        /** @brief The S4U actor */
        simgrid::s4u::ActorPtr s4u_actor;

        /** @brief The host on which the daemon is running */
        simgrid::s4u::Host *host;

        //        void release_held_mutexes();

        /**
         * @brief Method to retrieve the shared_ptr to a S4U_daemon
         * @tparam T: the class of the daemon (the base class is S4U_Daemon)
         * @return a shared pointer
         */
        template<class T>
        std::shared_ptr<T> getSharedPtr() {
            return std::dynamic_pointer_cast<T>(this->shared_from_this());
        }


        /** @brief The number of time that this daemon has started (i.e., 1 + number of restarts) */
        unsigned int num_starts = 0;

    private:
        // Lock used typically to prevent kill() from killing the actor
        // while it's in the middle of doing something critical
        simgrid::s4u::MutexPtr daemon_lock;


        bool has_returned_from_main_ = false;// Set to true after main returns
        int return_value_ = 0;               // Set to the value returned by main
        bool daemonized_=true;               // Whether the daemon is daemonized (will be overwritten by start())
        bool auto_restart_=false;            // Whether the daemon is supposed to auto-restart (will be overwritten by start())

        static int num_non_daemonized_actors_running;


#ifdef ACTOR_TRACKING_OUTPUT
        std::string process_name_prefix;
#endif
    };

    /***********************/
    /** \endcond           */
    /***********************/
}// namespace wrench


#include <boost/intrusive_ptr.hpp>
#include <simgrid/s4u/Io.hpp>
#include <functional>

namespace std {
  template<>
  struct hash<boost::intrusive_ptr<simgrid::s4u::Io>> {
    size_t operator()(const boost::intrusive_ptr<simgrid::s4u::Io>& ptr) const noexcept {
      return std::hash<void*>()(ptr.get());
    }
  };
}

#endif//WRENCH_SIM4U_DAEMON_H
