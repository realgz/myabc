// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/session/session_manager.cpp

#include "session_manager.hpp"

namespace myabc::engine {

SessionManager::SessionManager(LibPinyinEngine& engine, SourceRegistry& registry,
                               SessionOptions opts)
    : engine_(engine), registry_(registry), opts_(std::move(opts)) {}

Session& SessionManager::GetOrCreate(std::uint32_t session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        it = sessions_.emplace(session_id,
                               std::make_unique<Session>(session_id, engine_, registry_, opts_))
                 .first;
    }
    return *it->second;
}

void SessionManager::Remove(std::uint32_t session_id) { sessions_.erase(session_id); }

std::vector<std::uint32_t> SessionManager::SetSchemeForAll(InputScheme scheme) {
    opts_.scheme = scheme;
    std::vector<std::uint32_t> affected;
    for (auto& [id, session] : sessions_) {
        if (session->SetScheme(scheme)) affected.push_back(id);
    }
    return affected;
}

}  // namespace myabc::engine
