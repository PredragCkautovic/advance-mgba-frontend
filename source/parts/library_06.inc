        g.title = g.automaticTitle;
        g.shortTitle = metadata.shortTitle.empty() ? g.title : metadata.shortTitle;
        g.author = metadata.author;
        g.version = metadata.version;
        g.baseGame = metadata.baseGame;
        g.description = metadata.description.empty() ? descriptionFromSidecar(dir) : metadata.description;
        g.genres = metadata.genres;
        g.tags = metadata.tags;
        g.releaseYear = metadata.releaseYear;
        dedupe(g.genres);
        dedupe(g.tags);

        if (auto overrideIt = titleOverrides.find(g.path); overrideIt != titleOverrides.end() && !overrideIt->second.empty()) {
            g.title = overrideIt->second;
            if (metadata.shortTitle.empty()) g.shortTitle = g.title;
            g.titleSource = "Custom override";
            g.customTitle = true;
        }

        if (auto st = state.find(g.path); st != state.end()) {
            g.favorite = st->second.favorite;
            g.hidden = st->second.hidden;
            g.completed = st->second.completed;
            g.launches = st->second.launches;
            g.playSeconds = st->second.playSeconds;
            g.lastPlayed = st->second.lastPlayed;
            g.addedAt = st->second.addedAt;
        }
        if (g.addedAt <= 0) g.addedAt = fileTimestamp(rom);
        games.emplace_back(std::move(g));
    };

    if (config.scanRecursively) {
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            add(*it);
        }
    } else {
        for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            add(*it);
        }
    }

    std::sort(games.begin(), games.end(), [](const Game& a, const Game& b) {
        return normalizeName(a.title) < normalizeName(b.title);
    });
    return games;
}

} // namespace advance
