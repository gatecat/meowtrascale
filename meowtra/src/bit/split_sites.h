#ifndef SITE_H
#define SPLIT_SITES_H

#include "feature.h"
#include "tile.h"

MEOW_NAMESPACE_BEGIN

struct SplitSite {
	IdString site_type;
	pool<index_t> set_bits;
	pool<Feature> set_features;
};

std::vector<SplitSite> split_sites(Context *ctx, IdString tile_type, const pool<index_t> &set_bits, const pool<Feature> &features);

MEOW_NAMESPACE_END

#endif

