#ifndef SPECIMEN_H
#define SPECIMEN_H

#include "preface.h"
#include "feature.h"
#include "hashlib.h"

MEOW_NAMESPACE_BEGIN

// For the correlation
struct SpecimenData {
    SpecimenData(const pool<Feature> &features, const pool<index_t> &set_bits) : features(features), set_bits(set_bits) {};
    const pool<Feature> &features;
    const pool<index_t> &set_bits;
};

struct SpecimenGroup {
    std::vector<SpecimenData> specs;
    dict<Feature, pool<Feature>> dependencies;
    void find_deps();
};

MEOW_NAMESPACE_END

#endif
