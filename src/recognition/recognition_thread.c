#include "include/recognition_thread.h"

#include <assert.h>
#include <stddef.h>

void* RecognitionThread_run(void* arguments) {
    assert(arguments != NULL && "Arguments cannot be NULL");
    const RecognitionThreadArguments* recognition_thread_arguments =
        (RecognitionThreadArguments*)arguments;
    assert(recognition_thread_arguments != NULL &&
           "recognition_thread_arguments cannot be NULL");
    (void)recognition_thread_arguments;

    return NULL;
}
