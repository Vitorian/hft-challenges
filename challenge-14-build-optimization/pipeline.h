#pragma once
#include "types.h"

void init_pipeline();
void process_message(const Message& msg, Result& result);
void handle_special(const Message& msg, Result& result);
void run_pipeline(const Message* msgs, int count, Result* out);
