#pragma once
#include "types.h"

class BaseProcessor {
public:
    virtual ~BaseProcessor() = default;
    virtual void process(const Message& msg, Result& result) = 0;
};

BaseProcessor* create_processor();
