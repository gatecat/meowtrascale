#include "specimen.h"

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

MEOW_NAMESPACE_END
