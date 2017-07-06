/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "wrench-dev.h"
#include "services/ServiceMessage.h"
#include "services/storage_services/StorageServiceMessage.h"
#include "simgrid_S4U_util/S4U_Mailbox.h"
#include "IncomingFile.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(simple_storage_service, "Log category for Simple Storage Service");


namespace wrench {

    /**
    * @brief Generate a unique number
    *
    * @return a unique number
    */
    unsigned long SimpleStorageService::getNewUniqueNumber() {
      static unsigned long sequence_number = 0;
      return (sequence_number++);
    }

    /**
     * @brief Public constructor
     *
     * @param hostname: the name of the host on which to start the service
     * @param capacity: the storage capacity in bytes
     * @param plist: the optional property list
     */
    SimpleStorageService::SimpleStorageService(std::string hostname,
                                               double capacity,
                                               std::map<std::string, std::string> plist) :
            SimpleStorageService(hostname, capacity, plist, "_" + std::to_string(getNewUniqueNumber())) {}

    /**
     * @brief Private constructor
     *
     * @param hostname: the name of the host on which to start the service
     * @param capacity: the storage capacity in bytes
     * @param plist: the property list
     * @param suffix: the suffix (for the service name)
     *
     * @throw std::invalid_argument
     */
    SimpleStorageService::SimpleStorageService(
            std::string hostname,
            double capacity,
            std::map<std::string, std::string> plist,
            std::string suffix) :
            StorageService("simple_storage_service" + suffix, "simple_storage_service" + suffix, capacity) {

      // Set default properties
      for (auto p : this->default_property_values) {
        this->setProperty(p.first, p.second);
      }

      // Set specified properties
      for (auto p : plist) {
        this->setProperty(p.first, p.second);
      }

      // Start the daemon on the same host
      try {
        this->start(hostname);
      } catch (std::invalid_argument &e) {
        throw;
      }

    }

    /**
     * @brief Main method of the daemon
     *
     * @return 0 on termination
     */
    int SimpleStorageService::main() {

      TerminalOutput::setThisProcessLoggingColor(WRENCH_LOGGING_COLOR_CYAN);

      WRENCH_INFO("Simple Storage Service %s starting on host %s (capacity: %lf, holding %ld files, listening on %s)",
                  this->getName().c_str(),
                  S4U_Simulation::getHostName().c_str(),
                  this->capacity,
                  this->stored_files.size(),
                  this->mailbox_name.c_str());

      /** Main loop **/
      bool should_post_a_receive_on_my_standard_mailbox = true;
      bool should_continue = true;
      while (should_continue) {

        // Post a recv on my standard mailbox in case there is none pending
        if (should_post_a_receive_on_my_standard_mailbox) {
          S4U_PendingCommunication *comm = nullptr;
          try {
            comm = S4U_Mailbox::igetMessage(this->mailbox_name);
          } catch (std::runtime_error &e) {
            if (not strcmp(e.what(), "network_error")) {
              WRENCH_INFO("Can't even place an asynchronous get...something must be really wrong...aborting");
              break;
            } else {
              throw std::runtime_error(
                      "SimpleStorageService::waitForNextMessage(): Got an unknown exception while receiving a message.");
            }
          }
          this->pending_communications.push_back(comm);
          should_post_a_receive_on_my_standard_mailbox = false;
        }

        // Wait for a message
        WRENCH_INFO("Waiting for next message...");
        unsigned long target = S4U_PendingCommunication::waitForSomethingToHappen(this->pending_communications);

        // Extract the pending comm
        S4U_PendingCommunication *target_comm = this->pending_communications[target];
        this->pending_communications.erase(this->pending_communications.begin() + target);


        if (target_comm->comm_ptr->getMailbox()->getName() == this->mailbox_name) {
          should_continue = processControlMessage(target_comm);
          should_post_a_receive_on_my_standard_mailbox = true;
        } else {
          should_continue = processDataMessage(target_comm);
        }
      }

      WRENCH_INFO("Simple Storage Service %s on host %s terminated!",
                  this->getName().c_str(),
                  S4U_Simulation::getHostName().c_str());
      return 0;
    }


    /**
     * @brief Process a received control message
     *
     * @param comm: the pending communication
     * @return false if the daemon should terminate
     */
    bool SimpleStorageService::processControlMessage(S4U_PendingCommunication *comm) {

      // Get the message
      std::unique_ptr<SimulationMessage> message;
      try {
        message = comm->wait();
      } catch (std::exception &e) {
        WRENCH_INFO("Network error while receiving a control message... ignoring");
        return true;
      }

      if (message == nullptr) {
        WRENCH_INFO("Got a NULL message. This likely means that we're all done...Aborting!");
        return false;
      }

      WRENCH_INFO("Got a [%s] message", message->getName().c_str());

      if (ServiceStopDaemonMessage *msg = dynamic_cast<ServiceStopDaemonMessage *>(message.get())) {
        try {
          S4U_Mailbox::putMessage(msg->ack_mailbox,
                                  new ServiceDaemonStoppedMessage(this->getPropertyValueAsDouble(
                                          SimpleStorageServiceProperty::DAEMON_STOPPED_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          // Too bad...
        }
        return false;

      } else if (StorageServiceFreeSpaceRequestMessage *msg = dynamic_cast<StorageServiceFreeSpaceRequestMessage *>(message.get())) {
        double free_space = this->capacity - this->occupied_space;

        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                   new StorageServiceFreeSpaceAnswerMessage(free_space, this->getPropertyValueAsDouble(
                                           SimpleStorageServiceProperty::FREE_SPACE_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          // Too bad...
        }
        return true;

      } else if (StorageServiceFileDeleteRequestMessage *msg = dynamic_cast<StorageServiceFileDeleteRequestMessage *>(message.get())) {

        bool success = true;
        WorkflowExecutionFailureCause *failure_cause = nullptr;
        if (this->stored_files.find(msg->file) == this->stored_files.end()) {
          success = false;
          failure_cause = new FileNotFound(msg->file, this);
        } else {
          this->removeFileFromStorage(msg->file);
        }

        // Send an asynchronous reply
        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                   new StorageServiceFileDeleteAnswerMessage(msg->file,
                                                                             this,
                                                                             success,
                                                                             failure_cause,
                                                                             this->getPropertyValueAsDouble(
                                                                                     SimpleStorageServiceProperty::FILE_DELETE_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          // too bad, do nothing
        }

        return true;

      } else if (StorageServiceFileLookupRequestMessage *msg = dynamic_cast<StorageServiceFileLookupRequestMessage *>(message.get())) {

        bool file_found = (this->stored_files.find(msg->file) != this->stored_files.end());
        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                   new StorageServiceFileLookupAnswerMessage(msg->file, file_found,
                                                                             this->getPropertyValueAsDouble(
                                                                                     SimpleStorageServiceProperty::FILE_LOOKUP_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          // too bad, do nothing
        }

        return true;

      } else if (StorageServiceFileWriteRequestMessage *msg = dynamic_cast<StorageServiceFileWriteRequestMessage *>(message.get())) {

        return processFileWriteRequest(msg->file, msg->answer_mailbox);

      } else if (StorageServiceFileReadRequestMessage *msg = dynamic_cast<StorageServiceFileReadRequestMessage *>(message.get())) {

        return processFileReadRequest(msg->file, msg->answer_mailbox, msg->mailbox_to_receive_the_file_content);

      } else if (StorageServiceFileCopyRequestMessage *msg = dynamic_cast<StorageServiceFileCopyRequestMessage *>(message.get())) {

        return processFileCopyRequest(msg->file, msg->src, msg->answer_mailbox);

//        this->stored_files.insert(msg->file);
//        WRENCH_INFO("GOT A FILE!!!");
        // TODO #1: IF THIS COMMUNICATION FAILED, WE ALREADY UPDATED THE OCCUPIED SPACE, AND WE'LL THINK THE STORAGE IS FULLER THAN IT IS

        // TODO #2: And we MIGHT had to send ACKS to say "we're done"!!!
        //  TODO:  Create a map of "expected files"
        //  TODO:  That tells you what to do upon success and upon failure

        // QUESTION: SHOULD WE HAVE N MAILBOXES:
        //    - ONE FOR CONTROL, TO WHICH WE ALWAYS HAVE A getMessage
        //    - N * ONE FOR FILE RECEPTION, TO WHICH WE HAVE a number of CommPtr going on... to a temporary mailbox


      } else {
        throw std::runtime_error(
                "SimpleStorageService::processControlMessage(): Unexpected [" + message->getName() + "] message");
      }
    }

    /**
     * @brief Handle a file write request
     *
     * @param file: the file to write
     * @param answer_mailbox: the mailbox to which the reply should be sent
     * @return true if this process should keep running
     */
    bool SimpleStorageService::processFileWriteRequest(WorkflowFile *file, std::string answer_mailbox) {

      // Check the file size and capacity, and reply "no" if not enough space
      if (file->getSize() > (this->capacity - this->occupied_space)) {
        S4U_Mailbox::putMessage(answer_mailbox,
                                new StorageServiceFileWriteAnswerMessage(file,
                                                                         this,
                                                                         false,
                                                                         new StorageServiceFull(file, this),
                                                                         "",
                                                                         this->getPropertyValueAsDouble(
                                                                                 SimpleStorageServiceProperty::FILE_WRITE_ANSWER_MESSAGE_PAYLOAD)));
        return true;
      }

      // Update occupied space, in advance (will have to be decreased later in case of failure)
      this->occupied_space += file->getSize();

      // Generate a mailbox name on which to receive the file
      std::string file_reception_mailbox = S4U_Mailbox::generateUniqueMailboxName("file_reception");

      // Reply with a "go ahead, send me the file" message
      S4U_Mailbox::putMessage(answer_mailbox,
                              new StorageServiceFileWriteAnswerMessage(file,
                                                                       this,
                                                                       true,
                                                                       nullptr,
                                                                       file_reception_mailbox,
                                                                       this->getPropertyValueAsDouble(
                                                                               SimpleStorageServiceProperty::FILE_WRITE_ANSWER_MESSAGE_PAYLOAD)));

      S4U_PendingCommunication *pending_comm = nullptr;
      try {
        pending_comm = S4U_Mailbox::igetMessage(file_reception_mailbox);
      } catch (std::runtime_error &e) {
        if (not strcmp(e.what(), "network_error")) {
          WRENCH_INFO("Giving up on this message...");
          return true;
        } else {
          throw std::runtime_error(
                  "SimpleStorageService::processFileWriteRequest(): Got an unknown exception while receiving a file content.");
        }
      }

      // Add it to the list of pending_communications
      this->pending_communications.push_back(pending_comm);

      // Create an "incoming file" record
      this->incoming_files[pending_comm] = new IncomingFile(file, false, "");

      return true;
    }


    /**
     * @brief Handle a file read request
     * @param file: the file
     * @param answer_mailbox: the mailbox to which the answer should be sent
     * @param mailbox_to_receive_the_file_content: the mailbox to which the file will be sent
     * @return
     */
    bool SimpleStorageService::processFileReadRequest(WorkflowFile *file, std::string answer_mailbox, std::string mailbox_to_receive_the_file_content) {

      // Figure out whether this succeeds or not
      bool success = true;
      WorkflowExecutionFailureCause *failure_cause = nullptr;
      if (this->stored_files.
              find(file) == this->stored_files.
              end()) {
        WRENCH_INFO("Received a a read request for a file I don't have (%s)", this->getName().c_str());
        success = false;
        failure_cause = new FileNotFound(file, this);
      }

      // Send back the corresponding ack, asynchronously and in a "fire and forget" fashion
      try {
        S4U_Mailbox::dputMessage(answer_mailbox,
                                 new StorageServiceFileReadAnswerMessage(file, this, success, failure_cause,
                                                                         this->getPropertyValueAsDouble(
                                                                                 SimpleStorageServiceProperty::FILE_READ_ANSWER_MESSAGE_PAYLOAD)));
      } catch (std::runtime_error &e) {
        return true;
      }

      WRENCH_INFO("Asynchronously sending file %s to mailbox %s...", file->getId().c_str(), answer_mailbox.c_str());
      // If success, then follow up with sending the file (ASYNCHRONOUSLY!)
      if (success) {
        try {
          S4U_Mailbox::dputMessage(mailbox_to_receive_the_file_content, new
                  StorageServiceFileContentMessage(file));
        } catch (std::runtime_error &e) {
          return true;
        }
      }

      return true;
    }

    /**
     * @brief Handle a file copy request
     * @param file: the file
     * @param src: the storage service that holds the file
     * @param answer_mailbox: the mailbox to which the answer should be sent
     * @return
     */
    bool SimpleStorageService::processFileCopyRequest(WorkflowFile *file, StorageService *src, std::string answer_mailbox) {

      // Figure out whether this succeeds or not
      if (file->getSize() > this->capacity - this->occupied_space) {
        WRENCH_INFO("Cannot perform file copy due to lack of space");
        try {
          S4U_Mailbox::putMessage(answer_mailbox,
                                  new StorageServiceFileCopyAnswerMessage(file, this, false,
                                                                          new StorageServiceFull(file, this),
                                                                          this->getPropertyValueAsDouble(
                                                                                  SimpleStorageServiceProperty::FILE_COPY_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          return true;
        }
        return true;
      }

      WRENCH_INFO("Asynchronously copying file %s from storage service %s",
                  file->getId().c_str(),
                  src->getName().c_str());

      // Create a unique mailbox on which to receive the file
      std::string file_reception_mailbox = S4U_Mailbox::generateUniqueMailboxName("file_reception");

      // Initiate an ASYNCHRONOUS file read from the source
      try {
        src->initiateFileRead(file_reception_mailbox, file);
      } catch (WorkflowExecutionException &e) {
        try {
          S4U_Mailbox::putMessage(answer_mailbox,
                                  new StorageServiceFileCopyAnswerMessage(file, this, false, e.getCause(),
                                                                          this->getPropertyValueAsDouble(
                                                                                  SimpleStorageServiceProperty::FILE_COPY_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          return true;
        }
      }

      // Post the receive operation
      S4U_PendingCommunication *pending_comm;
      try {
        pending_comm = S4U_Mailbox::igetMessage(file_reception_mailbox);
      } catch (std::runtime_error &e) {
        return true;
      }

      // Update occupied space, in advance (will have to be decreased later in case of failure)
      this->occupied_space += file->getSize();

      // Add it to the list of pending_communications
      this->pending_communications.push_back(pending_comm);


      // Create an "incoming file" record
      this->incoming_files[pending_comm] = new IncomingFile(file, true, answer_mailbox);


      return true;
    }


    /**
    * @brief Process a received data message
    *
    * @param comm: the pending communication
    * @return false if the daemon should terminate
    *
    * @throw std::runtime_error
    */
    bool SimpleStorageService::processDataMessage(S4U_PendingCommunication *comm) {

      // Find the Incoming File record
      if (this->incoming_files.find(comm) == this->incoming_files.end()) {
        throw std::runtime_error("SimpleStorageService::processDataMessage(): Cannot find incoming file record for communications...");
      }

      IncomingFile *incoming_file = this->incoming_files[comm];
      this->incoming_files.erase(comm);

      // Get the message
      std::unique_ptr<SimulationMessage> message;
      try {
        message = comm->wait();
      } catch (std::exception &e) {
        WRENCH_INFO("SimpleStorageService::processDataMessage(): Communication failure when receiving file '%s", incoming_file->file->getId().c_str());
        // Process the failure, meaning, just re-decrease the occupied space
        this->occupied_space -= incoming_file->file->getSize();
        WRENCH_INFO("Sending back an ack since this was a file copy and some client is waiting for me to say something");
        try {
          S4U_Mailbox::putMessage(incoming_file->ack_mailbox,
                                  new StorageServiceFileCopyAnswerMessage(incoming_file->file, this, false,
                                                                          new NetworkError(),
                                                                          this->getPropertyValueAsDouble(
                                                                                  SimpleStorageServiceProperty::FILE_COPY_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::runtime_error &e) {
          WRENCH_INFO("Couldn't send ack... nevermind");
          return true;
        }
        return true;
      }

      if (message == nullptr) {
        WRENCH_INFO("Got a NULL message. This likely means that we're all done...Aborting!");
        return false;
      }

      WRENCH_INFO("Got a [%s] message", message->getName().c_str());

      if (StorageServiceFileContentMessage *msg = dynamic_cast<StorageServiceFileContentMessage *>(message.get())) {

        if (msg->file != incoming_file->file) {
          throw std::runtime_error("SimpleStorageService::processDataMessage(): Mismatch between received file and expected file... a bug in SimpleStorageService");
        }

        // Add the file to my storage

        this->stored_files.insert(incoming_file->file);

        // Send back the corresponding ack?
        if (incoming_file->send_file_copy_ack) {
          WRENCH_INFO("Sending back an ack since this was a file copy and some client is waiting for me to say something");
          try {
            S4U_Mailbox::putMessage(incoming_file->ack_mailbox,
                                    new StorageServiceFileCopyAnswerMessage(incoming_file->file, this, true, nullptr,
                                                                            this->getPropertyValueAsDouble(
                                                                                    SimpleStorageServiceProperty::FILE_COPY_ANSWER_MESSAGE_PAYLOAD)));
          } catch (std::runtime_error &e) {
            WRENCH_INFO("Couldn't send ack... nevermind");
            return true;
          }
        }


        return true;
      }  else {
        throw std::runtime_error(
                "SimpleStorageService::processControlMessage(): Unexpected [" + message->getName() + "] message");
      }

    }
};
