#pragma once
#include "types.h"

int classify_message(const Message& msg);
void route_message(const Message& msg, Result& result, int classification);
