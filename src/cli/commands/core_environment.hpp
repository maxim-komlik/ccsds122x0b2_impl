#pragma once

#include "io/tasking.hpp"

namespace cli::command {

struct execution_environment {
	task_pool pool;
};

extern execution_environment env;

}
