#include "dreadnought/strategy/mean_reversion.hpp"
#include "dreadnought/risk/risk_models.hpp"

namespace dreadnought {

// Explicit template instantiation
template class MeanReversionStrategy<StaticRiskModel>;
template class MeanReversionStrategy<DynamicRiskModel>;

} // namespace dreadnought

