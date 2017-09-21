//
// Created by suraj on 8/29/17.
//

#include "wrench/services/batch_service/BatchServiceProperty.h"

namespace wrench {
    SET_PROPERTY_NAME(BatchServiceProperty, THREAD_STARTUP_OVERHEAD);
    SET_PROPERTY_NAME(BatchServiceProperty, STANDARD_JOB_DONE_MESSAGE_PAYLOAD);
    SET_PROPERTY_NAME(BatchServiceProperty, STANDARD_JOB_FAILED_MESSAGE_PAYLOAD);
    SET_PROPERTY_NAME(BatchServiceProperty, SUBMIT_BATCH_JOB_ANSWER_MESSAGE_PAYLOAD);
    SET_PROPERTY_NAME(BatchServiceProperty, SUBMIT_BATCH_JOB_REQUEST_MESSAGE_PAYLOAD);
    SET_PROPERTY_NAME(BatchServiceProperty, HOST_SELECTION_ALGORITHM);
    SET_PROPERTY_NAME(BatchServiceProperty, JOB_SELECTION_ALGORITHM);
}