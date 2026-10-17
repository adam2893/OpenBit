// bl/play.h -- the interactive terminal driver: blsim --script SPEC.json --play.
//
// Tier 2 of PORT_PLAN: the same engine and the same tables as blsim --script,
// but each age-up is shown, its buttons are offered, and the branch the player
// picks is the branch the port executes. Nothing in the engine changes: a card
// already takes a forced 0-based button index (Sim::ageUp's third argument), so
// the driver's whole job is to show the card before the index is known.
#pragma once

#include "bl/script.h"

#include <string>

namespace bl {

// Runs one life as an interactive session. seed/country/gender and every setup
// switch come from the same ScriptSpec --script reads; spec.ages caps the run
// (0 means "until death"). Returns the process exit code.
int runPlayMode(const std::string& reDir, const ScriptSpec& spec);

}  // namespace bl
