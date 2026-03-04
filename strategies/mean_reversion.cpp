// Template instantiation for mean reversion strategy
#include "dreadnought/strategy/mean_reversion.hpp"
#include "dreadnought/risk/risk_models.hpp"

namespace dreadnought {

// Explicit template instantiation for common configurations
template class MeanReversionStrategy<StaticRiskModel>;
template class MeanReversionStrategy<DynamicRiskModel>;

} 