#pragma once
#include <memory>
#include <string>
#include "FrequencyAnalyzer.h"
#include "LogarithmicAnalyzer.h"
#include "LinearAnalyzer.h"
#include "BarkAnalyzer.h"
#include "MelAnalyzer.h"
#include "ThirdOctaveAnalyzer.h"
#include "FullOctaveAnalyzer.h"
#include "../config.h"

std::unique_ptr<FrequencyAnalyzer> getAnalyzer(AnalysisMode mode, FrequencyScale frequencyScale);
