#include "specimen.h"
#include <iostream>
MEOW_NAMESPACE_BEGIN

void SpecimenGroup::find_deps() {
    dependencies.clear();
    std::vector<Feature> to_erase;
    for (auto spec : specs) {
        for (auto feat : spec.features) {
            if (!dependencies.count(feat)) {
                // add all other set features in specimen as dependencies
                auto &deps = dependencies[feat];
                for (auto feat2 : spec.features) {
                    if (feat2 != feat)
                        deps.insert(feat2);
                }
            } else {
                // remove features not set as dependencies
                to_erase.clear();
                for (auto dep : dependencies.at(feat))
                    if (!spec.features.count(dep))
                        to_erase.push_back(dep);
                for (auto eliminated : to_erase)
                    dependencies.at(feat).erase(eliminated);
            }
        }
    }
}

void SpecimenGroup::solve(Context *ctx) {
    // sort by fewest dependencies first
    std::vector<Feature> to_solve;
    for (auto &dep : dependencies)
        to_solve.push_back(dep.first);
    std::stable_sort(to_solve.begin(), to_solve.end(), [&](Feature a, Feature b) {
        return dependencies.at(a).size() < dependencies.at(b).size();
    });
    dict<Feature, pool<index_t>> result;
    pool<index_t> set_with_feature;
    std::vector<index_t> to_remove;
    for (auto feat : to_solve) {
        bool found = false;
        set_with_feature.clear();
        for (auto &spec : specs) {
            if (!spec.features.count(feat))
                continue;
            if (!found) {
                // first time we hit feature, add all not set in dependent features
                for (auto set : spec.set_bits)
                    set_with_feature.insert(set);
                for (auto dep : dependencies.at(feat)) {
                    if (!result.count(dep))
                        continue;
                    for (auto dep_bit : result.at(dep))
                        set_with_feature.erase(dep_bit);
                }
            } else {
                to_remove.clear();
                // remove candidates not set in this specimen
                for (auto cand_bit : set_with_feature)
                    if (!spec.set_bits.count(cand_bit))
                        to_remove.push_back(cand_bit);
                for (auto unset_bit : to_remove)
                    set_with_feature.erase(unset_bit);
            }
            result[feat] = set_with_feature;
        }
    }
    for (auto &feat : result) {
        // TODO: save properly
        feat.first.write(ctx, std::cout);
        for (auto bit : feat.second)
            std::cout << " " << (bit / tile_bits) << "_" << (bit % tile_bits);
        std::cout << std::endl;
    }
}

MEOW_NAMESPACE_END
