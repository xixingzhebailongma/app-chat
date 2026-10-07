#include "channels/ChannelFactory.h"

#include <utility>

ChannelFactory& ChannelFactory::instance() {
    static ChannelFactory factory;
    return factory;
}

void ChannelFactory::registerBuilder(std::string name, Builder builder) {
    if (builder) {
        builders_[std::move(name)] = std::move(builder);
    }
}

std::shared_ptr<INotifyChannel> ChannelFactory::build(
    const std::string& name, const ChannelConfig& cfg,
    const ChannelDeps& deps) const {
    const auto it = builders_.find(name);
    if (it == builders_.end()) {
        return nullptr;
    }
    return it->second(cfg, deps);
}
