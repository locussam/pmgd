#pragma once

#include <functional>
#include "jarvis.h"

struct yy_params {
    Jarvis::Graph &db;
    std::function<void(Jarvis::Node &)> node_func;
    std::function<void(Jarvis::Edge &)> edge_func;
};