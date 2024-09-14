/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_FILE_COPY_COMPLETED_EVENT_H
#define WRENCH_FILE_COPY_COMPLETED_EVENT_H

#include "wrench/failure_causes/FailureCause.h"
#include <string>
#include <utility>

/***********************/
/** \cond DEVELOPER    */
/***********************/

namespace wrench {

class WorkflowTask;

class DataFile;

class StandardJob;

class PilotJob;

class ComputeService;

class StorageService;

class FileRegistryService;

class FileRegistryService;

/**
 * @brief A "file copy has completed" ExecutionEvent
 */
class FileCopyCompletedEvent : public ExecutionEvent {

private:
  friend class ExecutionEvent;
  /**
   * @brief Constructor
   * @param src: the source location
   * @param dst: the destination location
   */
  FileCopyCompletedEvent(std::shared_ptr<FileLocation> src,
                         std::shared_ptr<FileLocation> dst)
      : src(std::move(src)), dst(std::move(dst)) {}

public:
  /** @brief The source location */
  std::shared_ptr<FileLocation> src;
  /** @brief The destination location */
  std::shared_ptr<FileLocation> dst;

  /**
   * @brief Get a textual description of the event
   * @return a text string
   */
  std::string toString() override {
    return "FileCopyCompletedEvent (file: " + this->src->getFile()->getID() +
           "; src = " + this->src->toString() +
           "; dst = " + this->dst->toString() + ")";
  }
};

} // namespace wrench

/***********************/
/** \endcond           */
/***********************/

#endif // WRENCH_FILE_COPY_COMPLETED_EVENT_H
