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
    dict<Feature, index_t> feature_count;
    for (auto feat : to_solve) {
        bool found = false;
        set_with_feature.clear();
        for (auto &spec : specs) {
            if (!spec.features.count(feat))
                continue;
            ++feature_count[feat];
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
                found = true;
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
    // sort nicely
    std::vector<Feature> to_print(to_solve.begin(), to_solve.end());

    std::sort(to_print.begin(), to_print.end(), [&](const Feature &a, const Feature &b) {
        const auto &sa = a.base.str(ctx), &sb = b.base.str(ctx);
        return (sa < sb) || ((sa == sb) && a.bit < b.bit);
    });

    for (auto feat : to_print) {
        // TODO: save properly
        if (feature_count.at(feat) <= 1)
            continue; // too unrealiable to print...
        feat.write(ctx, std::cout);
        for (auto bit : result.at(feat))
            std::cout << " " << (bit / tile_bits) << "_" << (bit % tile_bits);
        std::cout << " # deps: " << dependencies.at(feat).size();
        std::cout << ", count: " << feature_count.at(feat);
        /*for (auto dep : dependencies.at(feat)) {
            dep.write(ctx, std::cout);
            std::cout << ", ";
        }*/
        std::cout << std::endl;
    }
}

MEOW_NAMESPACE_END
