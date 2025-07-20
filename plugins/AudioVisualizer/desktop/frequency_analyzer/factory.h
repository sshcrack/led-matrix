#pragma once
#include <memory>
#include "FrequencyAnalyzer.h"
#include "../config.h"

std::unique_ptr<FrequencyAnalyzer> getAnalyzer(AnalysisMode mode, FrequencyScale frequencyScale);
