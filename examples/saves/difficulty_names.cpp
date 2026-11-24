// The difficulty's names (spec 014's NNH_DATA_ENUM, used by spec 027 for enum checks and dumps): a saved
// difficulty outside Easy, Normal, Hard loads as the default and counts as dropped. Linked by the example
// only — not by nnh_tests, where a new NNH_DATA_ENUM would change spec 014's registry tests (plan 027, stage
// 2 refinements).

#include "progress.hpp"

using saves_example::Difficulty;
using saves_example::QuestState;

NNH_DATA_ENUM(Difficulty, Easy, Normal, Hard);
NNH_DATA_ENUM(QuestState, Open, Active, Done);
